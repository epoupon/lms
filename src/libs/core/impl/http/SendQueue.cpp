/*
 * Copyright (C) 2021 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SendQueue.hpp"

#include <algorithm>
#include <latch>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/error.hpp>

#include "core/Exception.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/String.hpp"

#define LOG(sev, message) LMS_LOG(HTTP, sev, "[Http SendQueue] - " << message)

namespace lms::core::stringUtils
{
    template<>
    std::optional<std::chrono::seconds> readAs(std::string_view str)
    {
        std::optional<std::chrono::seconds> res;

        if (const std::optional<std::size_t> value{ readAs<std::size_t>(str) })
            res = std::chrono::seconds{ *value };

        return res;
    }
} // namespace lms::core::stringUtils

namespace lms::core::http
{
    namespace
    {
        template<typename T>
        std::optional<T> headerReadAs(const Wt::Http::Message& msg, std::string_view headerName)
        {
            std::optional<T> res;

            if (const std::string * headerValue{ msg.getHeader(std::string{ headerName }) })
                res = stringUtils::readAs<T>(*headerValue);

            return res;
        }
    } // namespace

    SendQueue::SendQueue(boost::asio::io_context& ioContext, std::string_view baseUrl)
        : _ioContext{ ioContext }
        , _baseUrl{ baseUrl }
        , _abortAllRequests{ false }
        , _state{ State::Idle }
        , _client{ _ioContext }
    {
        _client.setFollowRedirect(true);
        _client.setTimeout(std::chrono::seconds{ 5 });

        // not very efficient (response bodies are copied for each callback), but Wt's code already makes copies anyway

        _client.bodyDataReceived().connect([this](const std::string& data) {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, data] {
                onClientBodyDataReceived(data);
            }));
        });

        _client.done().connect([this](Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg) {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, ec, msg = std::move(msg)] {
                onClientDone(ec, msg);
            }));
        });
    }

    SendQueue::~SendQueue()
    {
        abortAllRequests();
    }

    void SendQueue::abortAllRequests()
    {
        LOG(DEBUG, "Aborting all requests...");

        assert(!_abortAllRequests);
        _abortAllRequests = true;

        std::latch abortLatch{ 1 };

        boost::asio::post(boost::asio::bind_executor(_strand, [this, &abortLatch] {
            for (auto& [prio, requests] : _sendQueue)
            {
                while (!requests.empty())
                {
                    std::unique_ptr<ClientRequest> request{ std::move(requests.front()) };
                    requests.pop_front();
                    if (request->getParameters().onAbortFunc)
                        request->getParameters().onAbortFunc();
                }
            }

            if (_state == State::Throttled)
                _throttleTimer.cancel();
            else if (_state == State::Sending)
                _client.abort();

            abortLatch.count_down();
        }));

        abortLatch.wait();

        while (_state != State::Idle)
            std::this_thread::yield();

        _abortAllRequests = false;

        LOG(DEBUG, "All requests aborted!");
    }

    void SendQueue::sendRequest(std::unique_ptr<ClientRequest> request)
    {
        boost::asio::post(_strand, [this, request = std::move(request)]() mutable {
            if (_abortAllRequests)
            {
                LOG(DEBUG, "Not posting request because abortAllRequests() in progress");
                if (request->getParameters().onAbortFunc)
                    request->getParameters().onAbortFunc();

                return;
            }

            _sendQueue[request->getParameters().priority].emplace_back(std::move(request));

            if (_state == State::Idle)
                sendNextQueuedRequest();
        });
    }

    void SendQueue::sendNextQueuedRequest()
    {
        assert(_strand.running_in_this_thread());
        assert(!_currentRequest);

        for (auto& [prio, requests] : _sendQueue)
        {
            LOG(DEBUG, "Processing prio " << static_cast<int>(prio) << ", request count = " << requests.size());
            while (!requests.empty())
            {
                std::unique_ptr<ClientRequest> request{ std::move(requests.front()) };
                requests.pop_front();

                if (!sendRequest(*request))
                {
                    if (request->getParameters().onFailureFunc)
                        request->getParameters().onFailureFunc();
                    continue;
                }

                setState(State::Sending);
                _currentRequest = std::move(request);
                return;
            }
        }

        setState(State::Idle);
    }

    bool SendQueue::sendRequest(const ClientRequest& request)
    {
        assert(_strand.running_in_this_thread());

        LMS_SCOPED_TRACE_DETAILED("SendQueue", "SendRequest");

        const std::string url{ _baseUrl + request.getParameters().relativeUrl };
        LOG(DEBUG, "Sending " << (request.getType() == ClientRequest::Type::GET ? "GET" : "POST") << " request to url '" << url << "'");

        _client.setMaximumResponseSize(request.getParameters().onChunkReceived ? 0 : request.getParameters().responseBufferSize);

        bool res{};
        switch (request.getType())
        {
        case ClientRequest::Type::GET:
            res = _client.get(url, request.getGETParameters().headers);
            break;

        case ClientRequest::Type::POST:
            res = _client.post(url, request.getPOSTParameters().message);
            break;
        }

        if (!res)
            LOG(ERROR, "Send failed, bad url or unsupported scheme?");

        return res;
    }

    void SendQueue::onClientBodyDataReceived(const std::string& data)
    {
        assert(_strand.running_in_this_thread());
        assert(_currentRequest);

        if (_currentRequest->getParameters().onChunkReceived)
        {
            const auto byteSpan{ std::as_bytes(std::span{ data.data(), data.size() }) };
            if (_currentRequest->getParameters().onChunkReceived(byteSpan) == ClientRequestParameters::ChunckReceivedResult::Abort)
                _client.abort();
        }
    }

    void SendQueue::onClientDone(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg)
    {
        LMS_SCOPED_TRACE_DETAILED("SendQueue", "OnClientDone");

        assert(_currentRequest);

        LOG(DEBUG, "Client done. ec = " << ec.category().name() << " - " << ec.message() << " (" << ec.value() << "), status = " << msg.status());

        if (_abortAllRequests || ec == boost::asio::error::operation_aborted)
            onClientAborted(std::move(_currentRequest));
        else if (ec && (ec != boost::asio::ssl::error::stream_truncated))
            onClientDoneError(std::move(_currentRequest), ec);
        else
            onClientDoneSuccess(std::move(_currentRequest), msg);
    }

    void SendQueue::onClientAborted(std::unique_ptr<ClientRequest> request)
    {
        assert(_strand.running_in_this_thread());

        if (request->getParameters().onAbortFunc)
            request->getParameters().onAbortFunc();

        sendNextQueuedRequest();
    }

    void SendQueue::onClientDoneError(std::unique_ptr<ClientRequest> request, Wt::AsioWrapper::error_code ec)
    {
        assert(_strand.running_in_this_thread());

        LOG(WARNING, "Retry " << request->retryCount << ", client error: '" << ec.message() << "'");

        // may be a network error, try again later
        throttle(_defaultRetryWaitDuration);

        if (request->retryCount++ < _maxRetryCount)
        {
            _sendQueue[request->getParameters().priority].emplace_front(std::move(request));
        }
        else
        {
            LOG(ERROR, "Too many retries, giving up operation and throttle");
            if (request->getParameters().onFailureFunc)
                request->getParameters().onFailureFunc();
        }
    }

    void SendQueue::onClientDoneSuccess(std::unique_ptr<ClientRequest> request, const Wt::Http::Message& msg)
    {
        const ClientRequestParameters& requestParameters{ request->getParameters() };
        bool mustThrottle{};
        if (msg.status() == 429)
        {
            _sendQueue[requestParameters.priority].emplace_front(std::move(request));
            mustThrottle = true;
        }

        const auto remainingCount{ headerReadAs<std::size_t>(msg, "X-RateLimit-Remaining") };
        LOG(DEBUG, "Remaining messages = " << (remainingCount ? *remainingCount : 0));
        if (mustThrottle || (remainingCount && *remainingCount == 0))
        {
            const auto waitDuration{ headerReadAs<std::chrono::seconds>(msg, "X-RateLimit-Reset-In") };
            throttle(waitDuration.value_or(_defaultRetryWaitDuration));
        }

        if (!mustThrottle)
        {
            if (msg.status() == 200)
            {
                if (requestParameters.onSuccessFunc)
                    requestParameters.onSuccessFunc(msg);
            }
            else
            {
                LOG(ERROR, "Send error, status = " << msg.status() << ", body = '" << msg.body() << "'");
                if (requestParameters.onFailureFunc)
                    requestParameters.onFailureFunc();
            }
        }

        if (_state != State::Throttled)
            sendNextQueuedRequest();
    }

    void SendQueue::throttle(std::chrono::seconds requestedDuration)
    {
        const std::chrono::seconds duration{ std::clamp(requestedDuration, _minRetryWaitDuration, _maxRetryWaitDuration) };
        LOG(DEBUG, "Throttling for " << duration.count() << " seconds");

        _throttleTimer.expires_after(duration);
        _throttleTimer.async_wait([this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
                LOG(DEBUG, "Throttle aborted");
            else if (ec)
                throw LmsException{ "Throttle timer failure: " + std::string{ ec.message() } };

            setState(State::Idle);
            if (!ec)
                sendNextQueuedRequest();
        });

        setState(State::Throttled);
    }

    void SendQueue::setState(State state)
    {
        assert(_strand.running_in_this_thread());
        if (_state != state)
        {
            LOG(DEBUG, "Changing state to " << (state == State::Idle ? "Idle" : state == State::Sending ? "Sending" :
                                                                                                          "Throttled"));
            _state = state;
        }
    }
} // namespace lms::core::http
