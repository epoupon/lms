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

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>

#include "core/Exception.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/String.hpp"

#define LOG(sev, message) LMS_LOG(SCROBBLING, sev, "[Http SendQueue] - " << message)

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
    {
        _client.done().connect([this](Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg) {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, ec, msg = std::move(msg)] {
                onClientDone(ec, msg);
            }));
        });
    }

    SendQueue::~SendQueue()
    {
        _client.abort();
    }

    void SendQueue::sendRequest(std::unique_ptr<ClientRequest> request)
    {
        boost::asio::dispatch(_strand, [this, request = std::move(request)]() mutable {
            _sendQueue[request->getParameters().priority].emplace_back(std::move(request));

            if (_state == State::Idle)
                sendNextQueuedRequest();
        });
    }

    void SendQueue::sendNextQueuedRequest()
    {
        assert(_state == State::Idle);
        assert(!_currentRequest);

        for (auto& [prio, requests] : _sendQueue)
        {
            LOG(DEBUG, "Processing prio " << static_cast<int>(prio) << ", request count = " << requests.size());
            while (!requests.empty())
            {
                std::unique_ptr<ClientRequest> request{ std::move(requests.front()) };
                requests.pop_front();

                if (!sendRequest(*request))
                    continue;

                _state = State::Sending;
                _currentRequest = std::move(request);
                return;
            }
        }
    }

    bool SendQueue::sendRequest(const ClientRequest& request)
    {
        LMS_SCOPED_TRACE_DETAILED("SendQueue", "SendRequest");

        std::string url{ _baseUrl + request.getParameters().relativeUrl };
        LOG(DEBUG, "Sending request to url '" << url << "'");

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

    void SendQueue::onClientDone(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg)
    {
        LMS_SCOPED_TRACE_DETAILED("SendQueue", "OnClientDone");

        if (ec == boost::asio::error::operation_aborted)
        {
            LOG(DEBUG, "Client aborted");
            return;
        }

        assert(_currentRequest);
        _state = State::Idle;

        LOG(DEBUG, "Client done. status = " << msg.status());
        if (ec)
            onClientDoneError(std::move(_currentRequest), ec);
        else
            onClientDoneSuccess(std::move(_currentRequest), msg);
    }

    void SendQueue::onClientDoneError(std::unique_ptr<ClientRequest> request, Wt::AsioWrapper::error_code ec)
    {
        LOG(ERROR, "Retry " << request->retryCount << ", client error: '" << ec.message() << "'");

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
                    requestParameters.onSuccessFunc(msg.body());
            }
            else
            {
                LOG(ERROR, "Send error: '" << msg.body() << "'");
                if (requestParameters.onFailureFunc)
                    requestParameters.onFailureFunc();
            }
        }

        if (_state == State::Idle)
            sendNextQueuedRequest();
    }

    void SendQueue::throttle(std::chrono::seconds requestedDuration)
    {
        assert(_state == State::Idle);

        const std::chrono::seconds duration{ clamp(requestedDuration, _minRetryWaitDuration, _maxRetryWaitDuration) };
        LOG(DEBUG, "Throttling for " << duration.count() << " seconds");

        _throttleTimer.expires_after(duration);
        _throttleTimer.async_wait([this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
            {
                LOG(DEBUG, "Throttle aborted");
                return;
            }
            else if (ec)
            {
                throw LmsException{ "Throttle timer failure: " + std::string{ ec.message() } };
            }

            _state = State::Idle;
            sendNextQueuedRequest();
        });
        _state = State::Throttled;
    }
} // namespace lms::core::http
