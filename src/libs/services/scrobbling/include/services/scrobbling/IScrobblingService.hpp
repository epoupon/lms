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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include <Wt/WDateTime.h>
#include <boost/asio/io_context.hpp>

#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "services/scrobbling/Listen.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::scrobbling
{
    class IScrobblingService
    {
    public:
        virtual ~IScrobblingService() = default;

        using Clock = std::chrono::steady_clock;

        // Scrobbling

        // Notify that a listen has started (for now-playing purposes)
        virtual void listenStarted(const Listen& listen) = 0;

        // Notify that a listen has finished (for scrobbling purposes)
        virtual void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> playedDuration = std::nullopt) = 0;

        // Used to add listens afterwards (after some offline listening for example)
        virtual void addTimedListen(const TimedListen& listen) = 0;

        // Visit all now-playing listens
        virtual void visitNowPlayingListens(const std::function<void(Clock::time_point startedAt, const Listen&)>& visitor, db::UserId userId = {}) = 0;

        virtual void initiateLastFmLink(db::UserId userId,
                                        std::string_view apiKey,
                                        std::string_view apiSecret,
                                        std::function<void(std::string_view authUrl)> onSuccess,
                                        std::function<void()> onFailure)
            = 0;

        virtual void continueLastFmLink(db::UserId userId,
                                        std::function<void()> onSuccess,
                                        std::function<void()> onFailure)
            = 0;

        // Manually trigger an on-demand import of listen history from the given backend for this user (if supported)
        virtual void requestImmediateImport(db::UserId userId, db::ScrobblingBackend backend) = 0;

        // Manually trigger an on-demand export of all existing local listens to the given backend for this user (if supported)
        virtual void requestImmediateExport(db::UserId userId, db::ScrobblingBackend backend) = 0;

        // Stats
        virtual std::size_t getCount(db::UserId userId, db::ReleaseId releaseId) = 0;
        virtual std::size_t getCount(db::UserId userId, db::TrackId trackId) = 0;

        virtual Wt::WDateTime getLastListenDateTime(db::UserId userId, db::ReleaseId releaseId) = 0;
        virtual Wt::WDateTime getLastListenDateTime(db::UserId userId, db::TrackId trackId) = 0;
    };

    std::unique_ptr<IScrobblingService> createScrobblingService(boost::asio::io_context& ioContext, db::IDb& db);
} // namespace lms::scrobbling
