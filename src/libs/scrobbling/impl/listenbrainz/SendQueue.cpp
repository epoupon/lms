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

#include "utils/Logger.hpp"
#include "utils/String.hpp"

#define LOG(sev)	LMS_LOG(SCROBBLING, sev) << "[listenbrainz SendQueue] - "

namespace StringUtils
{
	template<>
	std::optional<std::chrono::seconds>
	readAs(const std::string& str)
	{
		std::optional<std::chrono::seconds> res;

		if (const std::optional<std::size_t> value {StringUtils::readAs<std::size_t>(str)})
			res = std::chrono::seconds {*value};

		return res;
	}
}

namespace
{
	template <typename T>
	std::optional<T>
	headerReadAs(const Wt::Http::Message& msg, std::string_view headerName)
	{
		std::optional<T> res;

		if (const std::string* headerValue {msg.getHeader(std::string {headerName})})
			res = StringUtils::readAs<T>(*headerValue);

		return res;
	}
}

namespace Scrobbling::ListenBrainz
{
	SendQueue::SendQueue(boost::asio::io_context& ioContext, std::string_view apiBaseURL)
	: _ioContext {ioContext}
	, _apiBaseURL {apiBaseURL}
	{
		_client.done().connect([this](Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg)
		{
			_strand.dispatch([=, msg = std::move(msg)]
			{
				onClientDone(ec, msg);
			});
		});
	}

	SendQueue::~SendQueue()
	{
		_client.abort();
	}

	void
	SendQueue::enqueueRequest(Request request)
	{
		_strand.dispatch([this, request = std::move(request)]()
		{
			_sendQueue[request._priority].emplace_back(std::move(request));

			if (_state == State::Idle)
				sendNextQueuedRequest();
		});
	}

	void
	SendQueue::sendNextQueuedRequest()
	{
		assert(_state == State::Idle);

		for (auto& [prio, requests] : _sendQueue)
		{
			LOG(DEBUG) << "Processing prio " << static_cast<int>(prio) << ", request count = " << requests.size();
			while (!requests.empty())
			{
				Request request {std::move(requests.front())};
				requests.pop_front();

				if (!sendRequest(request._requestData))
					continue;

				_state = State::Sending;
				_currentRequest = std::move(request);
				return;
			}
		}
	}

	bool
	SendQueue::sendRequest(const RequestData& requestData)
	{
		const std::string url {_apiBaseURL + requestData.endpoint};

		LOG(DEBUG) << "Sending request type " << (requestData.type == RequestData::Type::GET ? "GET" : "POST") << " to url '" << url << "'";

		bool res{};
		switch (requestData.type)
		{
			case RequestData::Type::GET:
				res = _client.get(url, requestData.headers);
				break;
			case RequestData::Type::POST:
				res = _client.post(url, requestData.message);
				break;
		}

		if (!res)
			LOG(ERROR) << "Send failed, bad url or unsupported scheme?";

		return res;
	}

	void
	SendQueue::onClientDone(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg)
	{
		if (ec == boost::asio::error::operation_aborted)
		{
			LOG(DEBUG) << "SendQueue: client aborted";
			return;
		}

		assert(_currentRequest);
		Request request {std::move(*_currentRequest)};
		_state = State::Idle;

		LOG(DEBUG) << "Client done. status = " << msg.status();
		if (ec)
		{
			LOG(ERROR) << "Retry " << request._retryCount << ", client error: '" << ec.message() << "'";

			// may be a network error, try again later
			throttle(_defaultRetryWaitDuration);

			if (request._retryCount++ < _maxRetryCount)
			{
				_sendQueue[request._priority].emplace_front(std::move(request));
			}
			else
			{
				LOG(ERROR) << "Too many retries, giving up operation and throttle";
				if (request._onFailureFunc)
					request._onFailureFunc();
			}
			return;
		}

		bool mustThrottle{};
		if (msg.status() == 429)
			_sendQueue[request._priority].emplace_front(std::move(request));

		const auto remainingCount {headerReadAs<std::size_t>(msg, "X-RateLimit-Remaining")};
		LOG(DEBUG) << "Remaining messages = " << (remainingCount ? *remainingCount : 0);
		if (mustThrottle || (remainingCount && *remainingCount == 0))
		{
			const auto waitDuration {headerReadAs<std::chrono::seconds>(msg, "X-RateLimit-Reset-In")};
			throttle(waitDuration.value_or(_defaultRetryWaitDuration));
		}

		if (!mustThrottle)
		{
			if (msg.status() == 200)
			{
				if (request._onSuccessFunc)
					request._onSuccessFunc(msg.body());
			}
			else
			{
				LOG(ERROR) << "Send error: '" << msg.body() << "'";
				if (request._onFailureFunc)
					request._onFailureFunc();
			}
		}

		if (_state == State::Idle)
			sendNextQueuedRequest();
	}

	void
	SendQueue::throttle(std::chrono::seconds requestedDuration)
	{
		assert(_state == State::Idle);

		const std::chrono::seconds duration {clamp(requestedDuration, _minRetryWaitDuration, _maxRetryWaitDuration)};
		LOG(DEBUG) << "Throttling for " << duration.count() << " seconds";

		_throttleTimer.expires_after(duration);
		_throttleTimer.async_wait([this](const boost::system::error_code& ec)
		{
			if (ec == boost::asio::error::operation_aborted)
			{
				LOG(DEBUG) << "SendQueue: throttle aborted";
				return;
			}

			if (ec)
				LOG(ERROR) << "async_wait failed:" << ec.message();

			_state = State::Idle;
			sendNextQueuedRequest();
		});
		_state = State::Throttled;
	}
} // namespace Scrobbling::ListenBrainz
