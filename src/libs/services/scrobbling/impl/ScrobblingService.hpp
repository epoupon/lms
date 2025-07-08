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

#include <memory>
#include <optional>
#include <unordered_map>

#include "services/scrobbling/IScrobblingService.hpp"

#include "IScrobblingBackend.hpp"

namespace lms::scrobbling
{
    class ScrobblingService : public IScrobblingService
    {
    public:
        ScrobblingService(boost::asio::io_context& ioContext, db::IDb& db);
        ~ScrobblingService();

    private:
        void listenStarted(const Listen& listen) override;
        void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration) override;
        void addTimedListen(const TimedListen& listen) override;

        ArtistContainer getRecentArtists(const ArtistFindParameters& params) override;
        ReleaseContainer getRecentReleases(const FindParameters& params) override;
        TrackContainer getRecentTracks(const FindParameters& params) override;

        std::size_t getCount(db::UserId userId, db::ReleaseId releaseId) override;
        std::size_t getCount(db::UserId userId, db::TrackId trackId) override;

        Wt::WDateTime getLastListenDateTime(db::UserId userId, db::ReleaseId releaseId) override;
        Wt::WDateTime getLastListenDateTime(db::UserId userId, db::TrackId trackId) override;

        ArtistContainer getTopArtists(const ArtistFindParameters& params) override;
        ReleaseContainer getTopReleases(const FindParameters& params) override;
        TrackContainer getTopTracks(const FindParameters& params) override;

        std::optional<db::ScrobblingBackend> getUserBackend(db::UserId userId);

        db::IDb& _db;
        std::unordered_map<db::ScrobblingBackend, std::unique_ptr<IScrobblingBackend>> _scrobblingBackends;
    };

} // namespace lms::scrobbling
