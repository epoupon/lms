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

#pragma once

#include <deque>

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/steady_timer.hpp>

#include <Wt/Http/Client.h>

namespace Scrobbling::ListenBrainz
{
	class SendQueue
	{
		public:
			SendQueue(boost::asio::io_context& ioContext, std::string_view apiBaseURL);
			~SendQueue();

			SendQueue(const SendQueue&) = delete;
			SendQueue(const SendQueue&&) = delete;
			SendQueue& operator=(const SendQueue&) = delete;
			SendQueue& operator=(const SendQueue&&) = delete;

			// generic queue operations
			struct RequestData
			{
				enum class Type
				{
					GET,
					POST,
				};

				Type type;
				std::string endpoint;	// relative URL to the base API
				std::vector<Wt::Http::Message::Header> headers; // used by GET
				Wt::Http::Message message; // used by POST
			};

			class Request
			{
				public:

					enum class Priority
					{
						High,
						Normal,
						Low,
					};

					Request(RequestData requestData) : _requestData {std::move(requestData)} {}

					using OnSuccessFunc = std::function<void(std::string_view msgBody)>;
					using OnFailureFunc = std::function<void()>;

					void setOnSuccessFunc(OnSuccessFunc onSuccessFunc) { _onSuccessFunc = onSuccessFunc; }
					void setOnFailureFunc(OnFailureFunc onFailureFunc) { _onFailureFunc = onFailureFunc; }
					void setPriority(Priority priority) { _priority = priority; }

				private:
					friend class SendQueue;
					RequestData	_requestData;
					Priority _priority {Priority::Normal};
					std::size_t	_retryCount {};
					OnSuccessFunc _onSuccessFunc;
					OnFailureFunc _onFailureFunc;
			};

			std::string_view getAPIBaseURL() const { return _apiBaseURL; }
			void enqueueRequest(Request request);

		private:
			void sendNextQueuedRequest();
			bool sendRequest(const RequestData& request);
			void onClientDone(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg);
			void onClientDoneError(Request request, Wt::AsioWrapper::error_code ec);
			void onClientDoneSuccess(Request request, const Wt::Http::Message& msg);
			void throttle(std::chrono::seconds duration);

			const std::size_t			_maxRetryCount {2};
			const std::chrono::seconds	_defaultRetryWaitDuration {30};
			const std::chrono::seconds	_minRetryWaitDuration {1};
			const std::chrono::seconds	_maxRetryWaitDuration {300};

			enum class State
			{
				Idle,
				Throttled,
				Sending,
			};
			boost::asio::io_context&		_ioContext;
			boost::asio::io_context::strand	_strand {_ioContext};
			boost::asio::steady_timer		_throttleTimer {_ioContext};
			std::string						_apiBaseURL;
			State							_state {State::Idle};
			Wt::Http::Client				_client {_ioContext};
			std::map<Request::Priority, std::deque<Request>> _sendQueue;
			std::optional<Request>			_currentRequest;
	};

} // namespace Scrobbling::ListenBrainz

