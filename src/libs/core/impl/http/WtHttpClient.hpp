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

#pragma once

#include <Wt/Http/Client.h>
#include <boost/asio/io_context.hpp>

#include "IWtHttpClient.hpp"

namespace lms::core::http
{
    // Production IWtHttpClient implementation, forwards to a real Wt::Http::Client.
    class WtHttpClient final : public IWtHttpClient
    {
    public:
        explicit WtHttpClient(boost::asio::io_context& ioContext);

    private:
        void setFollowRedirect(bool enable) override;
        void setTimeout(std::chrono::steady_clock::duration timeout) override;
        void setMaximumResponseSize(std::size_t bytes) override;

        bool get(const std::string& url, const std::vector<Wt::Http::Message::Header>& headers) override;
        bool post(const std::string& url, const Wt::Http::Message& message) override;
        void abort() override;

        Wt::Signal<std::string>& bodyDataReceived() override;
        Wt::Signal<Wt::AsioWrapper::error_code, Wt::Http::Message>& done() override;

        Wt::Http::Client _client;
    };
} // namespace lms::core::http
