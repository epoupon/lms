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
#include <shared_mutex>
#include <unordered_map>

#include "services/scrobbling/IScrobblingService.hpp"

#include "IScrobblingBackend.hpp"

namespace lms::scrobbling::lastFm
{
    class LastFmBackend;
}

namespace lms::scrobbling
{
    class ScrobblingService : public IScrobblingService
    {
    public:
        ScrobblingService(boost::asio::io_context& ioContext, db::IDb& db);
        ~ScrobblingService() override;

        ScrobblingService(const ScrobblingService&) = delete;
        ScrobblingService& operator=(const ScrobblingService&) = delete;

    private:
        void listenStarted(const Listen& listen) override;
        void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration) override;
        void addTimedListen(const TimedListen& listen) override;
        void visitNowPlayingListens(const std::function<void(Clock::time_point startedAt, const Listen&)>& visitor, db::UserId userId) override;

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

        void initiateLastFmLink(db::UserId userId, std::string_view apiKey, std::string_view apiSecret,
                                std::function<void(std::string_view authUrl)> onSuccess,
                                std::function<void()> onFailure) override;

        void continueLastFmLink(db::UserId userId,
                                std::function<void()> onSuccess,
                                std::function<void()> onFailure) override;

        std::optional<db::ScrobblingBackend> getUserBackend(db::UserId userId);

        void insertNowPlayingEntry(const Listen& listen);

        db::IDb& _db;
        std::unordered_map<db::ScrobblingBackend, std::unique_ptr<IScrobblingBackend>> _scrobblingBackends;
        lastFm::LastFmBackend* _lastFmBackend{}; // non-owning, owned via _scrobblingBackends

        std::shared_mutex _nowPlayingEntriesMutex;
        struct NowPlayingEntry
        {
            Clock::time_point startedAt;
            Clock::time_point expiryAt;
            db::TrackId trackId;
        };
        std::unordered_map<db::UserId, NowPlayingEntry> _nowPlayingEntries;
    };
} // namespace lms::scrobbling
