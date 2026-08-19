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

#include "WtHttpClient.hpp"

namespace lms::core::http
{
    WtHttpClient::WtHttpClient(boost::asio::io_context& ioContext)
        : _client{ ioContext }
    {
    }

    void WtHttpClient::setFollowRedirect(bool enable)
    {
        _client.setFollowRedirect(enable);
    }

    void WtHttpClient::setTimeout(std::chrono::steady_clock::duration timeout)
    {
        _client.setTimeout(timeout);
    }

    void WtHttpClient::setMaximumResponseSize(std::size_t bytes)
    {
        _client.setMaximumResponseSize(bytes);
    }

    bool WtHttpClient::get(const std::string& url, const std::vector<Wt::Http::Message::Header>& headers)
    {
        return _client.get(url, headers);
    }

    bool WtHttpClient::post(const std::string& url, const Wt::Http::Message& message)
    {
        return _client.post(url, message);
    }

    void WtHttpClient::abort()
    {
        _client.abort();
    }

    Wt::Signal<std::string>& WtHttpClient::bodyDataReceived()
    {
        return _client.bodyDataReceived();
    }

    Wt::Signal<Wt::AsioWrapper::error_code, Wt::Http::Message>& WtHttpClient::done()
    {
        return _client.done();
    }
} // namespace lms::core::http
