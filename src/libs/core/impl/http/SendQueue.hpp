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

#include <atomic>
#include <deque>
#include <memory>
#include <string_view>

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/steady_timer.hpp>

#include "ClientRequest.hpp"
#include "IWtHttpClient.hpp"

namespace lms::core::http
{
    // Retry/throttle timings
    struct RetryPolicy
    {
        std::size_t maxRetryCount{ 2 };
        std::chrono::seconds defaultRetryWaitDuration{ 30 };
        std::chrono::seconds minRetryWaitDuration{ 1 };
        std::chrono::seconds maxRetryWaitDuration{ 300 };
    };

    class SendQueue
    {
    public:
        SendQueue(boost::asio::io_context& ioContext, std::string_view baseUrl);
        SendQueue(boost::asio::io_context& ioContext, std::string_view baseUrl, std::unique_ptr<IWtHttpClient> httpClient, RetryPolicy retryPolicy = {});
        ~SendQueue();

        SendQueue(const SendQueue&) = delete;
        SendQueue(const SendQueue&&) = delete;
        SendQueue& operator=(const SendQueue&) = delete;
        SendQueue& operator=(const SendQueue&&) = delete;

        void sendRequest(std::unique_ptr<ClientRequest> request);
        void abortAllRequests();

    private:
        void sendNextQueuedRequest();
        bool sendRequest(const ClientRequest& request);
        void onClientBodyDataReceived(const std::string& data);
        void onClientAborted(std::unique_ptr<ClientRequest> request);
        void onClientDone(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg);
        void onClientDoneError(std::unique_ptr<ClientRequest> request, Wt::AsioWrapper::error_code ec);
        void onClientDoneSuccess(std::unique_ptr<ClientRequest> request, const Wt::Http::Message& msg);
        void throttle(std::chrono::seconds duration);

        const RetryPolicy _retryPolicy;

        boost::asio::io_context& _ioContext;
        boost::asio::io_context::strand _strand{ _ioContext }; // protect _state, _sendQueue and _currentRequest
        boost::asio::steady_timer _throttleTimer{ _ioContext };
        const std::string _baseUrl;

        enum class State
        {
            Idle,
            Throttled,
            Sending,
        };
        void setState(State state);
        std::atomic<bool> _abortAllRequests;
        State _state;
        std::unique_ptr<IWtHttpClient> _httpClient;
        std::map<ClientRequestParameters::Priority, std::deque<std::unique_ptr<ClientRequest>>> _sendQueue;
        std::unique_ptr<ClientRequest> _currentRequest;
    };
} // namespace lms::core::http