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
#include "IScrobbler.hpp"

namespace Scrobbling
{
    class ScrobblingService : public IScrobblingService
    {
    public:
        ScrobblingService(boost::asio::io_context& ioContext, Database::Db& db);
        ~ScrobblingService();

    private:
        void listenStarted(const Listen& listen) override;
        void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration) override;
        void addTimedListen(const TimedListen& listen) override;

        ArtistContainer getRecentArtists(Database::UserId userId,
            const std::vector<Database::ClusterId>& clusterIds,
            std::optional<Database::TrackArtistLinkType> linkType,
            Database::Range range) override;

        ReleaseContainer getRecentReleases(Database::UserId userId,
            const std::vector<Database::ClusterId>& clusterIds,
            Database::Range range) override;

        TrackContainer getRecentTracks(Database::UserId userId,
            const std::vector<Database::ClusterId>& clusterIds,
            Database::Range range) override;

        Wt::WDateTime getLastListenDateTime(Database::UserId userId, Database::ReleaseId releaseId) override;
        Wt::WDateTime getLastListenDateTime(Database::UserId userId, Database::TrackId trackId) override;

        ArtistContainer getTopArtists(Database::UserId userId,
            const std::vector<Database::ClusterId>& clusterIds,
            std::optional<Database::TrackArtistLinkType> linkType,
            Database::Range range) override;

        ReleaseContainer getTopReleases(Database::UserId userId,
            const std::vector<Database::ClusterId>& clusterIds,
            Database::Range range) override;

        TrackContainer getTopTracks(Database::UserId userId,
            const std::vector<Database::ClusterId>& clusterIds,
            Database::Range range) override;

        void star(Database::UserId userId, Database::ArtistId artistId) override;
        void unstar(Database::UserId userId, Database::ArtistId artistId) override;
        bool isStarred(Database::UserId userId, Database::ArtistId artistId) override;
        Wt::WDateTime getStarredDateTime(Database::UserId userId, Database::ArtistId artistId) override;
        ArtistContainer	getStarredArtists(Database::UserId userId,
            const std::vector<Database::ClusterId>& clusterIds,
            std::optional<Database::TrackArtistLinkType> linkType,
            Database::ArtistSortMethod sortMethod,
            Database::Range range) override;

        void star(Database::UserId userId, Database::ReleaseId releaseId) override;
        void unstar(Database::UserId userId, Database::ReleaseId releaseId) override;
        bool isStarred(Database::UserId userId, Database::ReleaseId releasedId) override;
        Wt::WDateTime getStarredDateTime(Database::UserId userId, Database::ReleaseId releasedId) override;
        ReleaseContainer getStarredReleases(Database::UserId userId, const std::vector<Database::ClusterId>& clusterIds, Database::Range range) override;

        void star(Database::UserId userId, Database::TrackId trackId) override;
        void unstar(Database::UserId userId, Database::TrackId trackId) override;
        bool isStarred(Database::UserId userId, Database::TrackId trackId) override;
        Wt::WDateTime getStarredDateTime(Database::UserId userId, Database::TrackId trackId) override;
        TrackContainer getStarredTracks(Database::UserId userId, const std::vector<Database::ClusterId>& clusterIds, Database::Range range) override;

        std::optional<Database::Scrobbler> getUserScrobbler(Database::UserId userId);

        template <typename ObjType, typename ObjIdType, typename StarredObjType>
        void star(Database::UserId userId, ObjIdType id);
        template <typename ObjType, typename ObjIdType, typename StarredObjType>
        void unstar(Database::UserId userId, ObjIdType id);
        template <typename ObjType, typename ObjIdType, typename StarredObjType>
        bool isStarred(Database::UserId userId, ObjIdType id);
        template <typename ObjType, typename ObjIdType, typename StarredObjType>
        Wt::WDateTime getStarredDateTime(Database::UserId userId, ObjIdType id);

        Database::Db& _db;
        std::unordered_map<Database::Scrobbler, std::unique_ptr<IScrobbler>> _scrobblers;
    };

} // ns Scrobbling

