/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <boost/asio/io_context.hpp>

#include "database/objects/UserId.hpp"

#include "IScrobblingBackend.hpp"
#include "ScrobblingsSynchronizer.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::scrobbling::lastFm
{
    class LastFmBackend final : public IScrobblingBackend
    {
    public:
        LastFmBackend(boost::asio::io_context& ioContext, db::IDb& db);
        ~LastFmBackend() override;

        void initiateLastFmLink(db::UserId userId,
                                std::string_view apiKey,
                                std::string_view apiSecret,
                                std::function<void(std::string_view authUrl)> onSuccess,
                                std::function<void()> onFailure);

        void continueLastFmLink(db::UserId userId,
                                std::function<void()> onSuccess,
                                std::function<void()> onFailure);

    private:
        LastFmBackend(const LastFmBackend&) = delete;
        LastFmBackend& operator=(const LastFmBackend&) = delete;

        void listenStarted(const Listen& listen) override;
        void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> playedDuration) override;
        void addTimedListen(const TimedListen& listen) override;

        struct PendingAuth
        {
            std::string apiKey;
            std::string apiSecret;
            std::string token;
        };

        db::IDb& _db;
        const std::string _authBaseUrl;
        std::unique_ptr<core::http::IClient> _client;
        ScrobblingsSynchronizer _synchronizer;

        std::mutex _pendingAuthsMutex;
        std::unordered_map<db::UserId, PendingAuth> _pendingAuths;
    };
} // namespace lms::scrobbling::lastFm
