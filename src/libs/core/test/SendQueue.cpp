/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <span>
#include <thread>

#include <gtest/gtest.h>

#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/error.hpp>

#include "http/IWtHttpClient.hpp"
#include "http/SendQueue.hpp"

namespace lms::core::http::tests
{
    namespace
    {
        class StubWtHttpClient : public IWtHttpClient
        {
        public:
            struct Call
            {
                enum class Type
                {
                    Get,
                    Post
                };
                Type type;
                std::string url;
            };

            void setFollowRedirect(bool) override {}
            void setTimeout(std::chrono::steady_clock::duration) override {}
            void setMaximumResponseSize(std::size_t) override {}

            bool get(const std::string& url, const std::vector<Wt::Http::Message::Header>&) override
            {
                _calls.push_back(Call{ .type = Call::Type::Get, .url = url });
                return true;
            }

            bool post(const std::string& url, const Wt::Http::Message&) override
            {
                _calls.push_back(Call{ .type = Call::Type::Post, .url = url });
                return true;
            }

            void abort() override
            {
                // contract: the abort completes asynchronously by emitting done() with operation_aborted
                _done.emit(boost::asio::error::operation_aborted, Wt::Http::Message{});
            }

            Wt::Signal<std::string>& bodyDataReceived() override { return _bodyDataReceived; }
            Wt::Signal<Wt::AsioWrapper::error_code, Wt::Http::Message>& done() override { return _done; }

            void simulateBodyData(const std::string& data) { _bodyDataReceived.emit(data); }
            void simulateDone(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg) { _done.emit(ec, msg); }

            std::span<const Call> getCalls() const { return _calls; }

        private:
            Wt::Signal<std::string> _bodyDataReceived;
            Wt::Signal<Wt::AsioWrapper::error_code, Wt::Http::Message> _done;
            std::vector<Call> _calls;
        };

        std::unique_ptr<ClientRequest> makeGetRequest(std::string relativeUrl, bool& successCalled, bool& failureCalled)
        {
            ClientGETRequestParameters params;
            params.relativeUrl = std::move(relativeUrl);
            params.onSuccessFunc = [&successCalled](const Wt::Http::Message&) { successCalled = true; };
            params.onFailureFunc = [&failureCalled] { failureCalled = true; };
            return std::make_unique<ClientRequest>(std::move(params));
        }

        class SendQueueTest : public ::testing::Test
        {
        protected:
            static constexpr RetryPolicy fastRetryPolicy{
                .maxRetryCount{ 2 },
                .defaultRetryWaitDuration{ std::chrono::seconds{ 0 } },
                .minRetryWaitDuration{ std::chrono::seconds{ 0 } },
                .maxRetryWaitDuration{ std::chrono::seconds{ 0 } },
            };

            SendQueueTest()
            {
                auto httpClient{ std::make_unique<StubWtHttpClient>() };
                _httpClient = httpClient.get();
                _queue = std::make_unique<SendQueue>(_ioContext, "http://example.com", std::move(httpClient), fastRetryPolicy);
            }

            ~SendQueueTest() override
            {
                auto workGuard{ boost::asio::make_work_guard(_ioContext) };
                std::thread runner{ [this] {
                    _ioContext.restart();
                    _ioContext.run();
                } };
                _queue.reset();
                workGuard.reset();
                runner.join();
            }

            SendQueueTest(const SendQueueTest&) = delete;
            SendQueueTest& operator=(const SendQueueTest&) = delete;

            void pump()
            {
                _ioContext.restart();
                _ioContext.run();
            }

            boost::asio::io_context _ioContext;
            StubWtHttpClient* _httpClient{};
            std::unique_ptr<SendQueue> _queue;
        };
    } // namespace

    TEST_F(SendQueueTest, CleanSuccess)
    {
        bool successCalled{};
        bool failureCalled{};
        _queue->sendRequest(makeGetRequest("/ok", successCalled, failureCalled));
        pump();
        ASSERT_EQ(_httpClient->getCalls().size(), 1u);

        Wt::Http::Message msg;
        msg.setStatus(200);
        _httpClient->simulateDone({}, msg);
        pump();

        EXPECT_TRUE(successCalled);
        EXPECT_FALSE(failureCalled);
        EXPECT_EQ(_httpClient->getCalls().size(), 1u);
    }

    TEST_F(SendQueueTest, StreamTruncatedButVerifiedComplete)
    {
        bool successCalled{};
        bool failureCalled{};
        _queue->sendRequest(makeGetRequest("/ok", successCalled, failureCalled));
        pump();
        ASSERT_EQ(_httpClient->getCalls().size(), 1u);

        _httpClient->simulateBodyData("0123456789"); // 10 bytes
        pump();

        Wt::Http::Message msg;
        msg.setStatus(200);
        msg.setHeader("Content-Length", "10");
        _httpClient->simulateDone(boost::asio::ssl::error::stream_truncated, msg);
        pump();

        EXPECT_TRUE(successCalled);
        EXPECT_FALSE(failureCalled);
        EXPECT_EQ(_httpClient->getCalls().size(), 1u); // verified complete: no retry
    }

    TEST_F(SendQueueTest, StreamTruncatedNon200StatusFailsWithoutRetry)
    {
        bool successCalled{};
        bool failureCalled{};
        _queue->sendRequest(makeGetRequest("/ok", successCalled, failureCalled));
        pump();
        ASSERT_EQ(_httpClient->getCalls().size(), 1u);

        Wt::Http::Message msg;
        msg.setStatus(302);
        msg.setHeader("Content-Length", "0");
        _httpClient->simulateDone(boost::asio::ssl::error::stream_truncated, msg);
        pump();

        EXPECT_FALSE(successCalled);
        EXPECT_TRUE(failureCalled);
        EXPECT_EQ(_httpClient->getCalls().size(), 1u); // verified complete but non-200: fails immediately, no retry
    }

    TEST_F(SendQueueTest, StreamTruncatedIncompleteRetriesThenSucceeds)
    {
        bool successCalled{};
        bool failureCalled{};
        _queue->sendRequest(makeGetRequest("/ok", successCalled, failureCalled));
        pump();
        ASSERT_EQ(_httpClient->getCalls().size(), 1u);

        _httpClient->simulateBodyData("0123"); // only 4 of the promised 10 bytes arrived
        pump();

        Wt::Http::Message truncatedMsg;
        truncatedMsg.setStatus(200);
        truncatedMsg.setHeader("Content-Length", "10");
        _httpClient->simulateDone(boost::asio::ssl::error::stream_truncated, truncatedMsg);
        pump();

        EXPECT_FALSE(successCalled);
        EXPECT_FALSE(failureCalled);
        ASSERT_EQ(_httpClient->getCalls().size(), 2u); // could not verify completeness: retried

        _httpClient->simulateBodyData("0123456789");
        pump();

        Wt::Http::Message okMsg;
        okMsg.setStatus(200);
        okMsg.setHeader("Content-Length", "10");
        _httpClient->simulateDone({}, okMsg);
        pump();

        EXPECT_TRUE(successCalled);
        EXPECT_FALSE(failureCalled);
    }

    TEST_F(SendQueueTest, StreamTruncatedWithNoContentLengthAlwaysRetries)
    {
        bool successCalled{};
        bool failureCalled{};
        _queue->sendRequest(makeGetRequest("/ok", successCalled, failureCalled));
        pump();
        ASSERT_EQ(_httpClient->getCalls().size(), 1u);

        Wt::Http::Message msg;
        msg.setStatus(200); // no Content-Length header at all: can't verify completeness
        _httpClient->simulateDone(boost::asio::ssl::error::stream_truncated, msg);
        pump();

        EXPECT_FALSE(successCalled);
        EXPECT_FALSE(failureCalled);
        EXPECT_EQ(_httpClient->getCalls().size(), 2u);
    }

    TEST_F(SendQueueTest, OrdinaryTransientErrorRetriesThenGivesUp)
    {
        bool successCalled{};
        bool failureCalled{};
        _queue->sendRequest(makeGetRequest("/ok", successCalled, failureCalled));
        pump();

        // maxRetryCount = 2: expect 3 total attempts (1 initial + 2 retries) before giving up
        for (std::size_t attempt = 1; attempt <= 3; ++attempt)
        {
            ASSERT_EQ(_httpClient->getCalls().size(), attempt);
            Wt::Http::Message msg;
            _httpClient->simulateDone(boost::asio::error::connection_reset, msg);
            pump();
        }

        EXPECT_FALSE(successCalled);
        EXPECT_TRUE(failureCalled);
        EXPECT_EQ(_httpClient->getCalls().size(), 3u); // no further retry beyond maxRetryCount
    }
} // namespace lms::core::http::tests
