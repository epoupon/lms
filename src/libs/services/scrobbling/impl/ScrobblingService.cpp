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

#include "ScrobblingService.hpp"
#include "ScrobblingService.impl.hpp"

#include "services/database/Artist.hpp"
#include "services/database/Db.hpp"
#include "services/database/Listen.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/StarredArtist.hpp"
#include "services/database/StarredRelease.hpp"
#include "services/database/StarredTrack.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/Logger.hpp"

#include "internal/InternalScrobbler.hpp"
#include "listenbrainz/ListenBrainzScrobbler.hpp"

namespace Scrobbling
{
    using namespace Database;

    std::unique_ptr<IScrobblingService> createScrobblingService(boost::asio::io_context& ioContext, Db& db)
    {
        return std::make_unique<ScrobblingService>(ioContext, db);
    }

    ScrobblingService::ScrobblingService(boost::asio::io_context& ioContext, Db& db)
        : _db{ db }
    {
        LMS_LOG(SCROBBLING, INFO) << "Starting service...";
        _scrobblers.emplace(Scrobbler::Internal, std::make_unique<InternalScrobbler>(_db));
        _scrobblers.emplace(Scrobbler::ListenBrainz, std::make_unique<ListenBrainz::Scrobbler>(ioContext, _db));
        LMS_LOG(SCROBBLING, INFO) << "Service started!";
    }

    ScrobblingService::~ScrobblingService()
    {
        LMS_LOG(SCROBBLING, INFO) << "Service stopped!";
    }

    void ScrobblingService::listenStarted(const Listen& listen)
    {
        if (std::optional<Scrobbler> scrobbler{ getUserScrobbler(listen.userId) })
            _scrobblers[*scrobbler]->listenStarted(listen);
    }

    void ScrobblingService::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
    {
        if (std::optional<Scrobbler> scrobbler{ getUserScrobbler(listen.userId) })
            _scrobblers[*scrobbler]->listenFinished(listen, duration);
    }

    void ScrobblingService::addTimedListen(const TimedListen& listen)
    {
        if (std::optional<Scrobbler> scrobbler{ getUserScrobbler(listen.userId) })
            _scrobblers[*scrobbler]->addTimedListen(listen);
    }

    std::optional<Scrobbler> ScrobblingService::getUserScrobbler(UserId userId)
    {
        std::optional<Scrobbler> scrobbler;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };
        if (const User::pointer user{ User::find(session, userId) })
            scrobbler = user->getScrobbler();

        return scrobbler;
    }

    ScrobblingService::ArtistContainer ScrobblingService::getRecentArtists(UserId userId, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, Range range)
    {
        ArtistContainer res;

        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        res = Database::Listen::getRecentArtists(session, userId, *scrobbler, clusterIds, linkType, range);
        return res;
    }

    ScrobblingService::ReleaseContainer ScrobblingService::getRecentReleases(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        ReleaseContainer res;

        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        res = Database::Listen::getRecentReleases(session, userId, *scrobbler, clusterIds, range);
        return res;
    }

    ScrobblingService::TrackContainer ScrobblingService::getRecentTracks(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        TrackContainer res;

        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        res = Database::Listen::getRecentTracks(session, userId, *scrobbler, clusterIds, range);
        return res;
    }

    Wt::WDateTime ScrobblingService::getLastListenDateTime(Database::UserId userId, Database::ReleaseId releaseId)
    {
        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return {};

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        const Database::Listen::pointer listen{ Database::Listen::getMostRecentListen(session, userId, *scrobbler, releaseId) };
        return listen ? listen->getDateTime() : Wt::WDateTime{};
    }

    Wt::WDateTime ScrobblingService::getLastListenDateTime(Database::UserId userId, Database::TrackId trackId)
    {
        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return {};

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        const Database::Listen::pointer listen{ Database::Listen::getMostRecentListen(session, userId, *scrobbler, trackId) };
        return listen ? listen->getDateTime() : Wt::WDateTime{};
    }

    // Top
    ScrobblingService::ArtistContainer ScrobblingService::getTopArtists(UserId userId, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, Range range)
    {
        ArtistContainer res;

        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        res = Database::Listen::getTopArtists(session, userId, *scrobbler, clusterIds, linkType, range);
        return res;
    }

    ScrobblingService::ReleaseContainer ScrobblingService::getTopReleases(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        ReleaseContainer res;

        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        res = Database::Listen::getTopReleases(session, userId, *scrobbler, clusterIds, range);
        return res;
    }

    ScrobblingService::TrackContainer ScrobblingService::getTopTracks(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        TrackContainer res;

        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        res = Database::Listen::getTopTracks(session, userId, *scrobbler, clusterIds, range);
        return res;
    }

    void ScrobblingService::star(UserId userId, ArtistId artistId)
    {
        star<Artist, ArtistId, StarredArtist>(userId, artistId);
    }

    void ScrobblingService::unstar(UserId userId, ArtistId artistId)
    {
        unstar<Artist, ArtistId, StarredArtist>(userId, artistId);
    }

    bool ScrobblingService::isStarred(UserId userId, ArtistId artistId)
    {
        return isStarred<Artist, ArtistId, StarredArtist>(userId, artistId);
    }

    Wt::WDateTime ScrobblingService::getStarredDateTime(UserId userId, ArtistId artistId)
    {
        return getStarredDateTime<Artist, ArtistId, StarredArtist>(userId, artistId);
    }

    ScrobblingService::ArtistContainer ScrobblingService::getStarredArtists(UserId userId, const std::vector<ClusterId>& clusterIds,
        std::optional<TrackArtistLinkType> linkType,
        ArtistSortMethod sortMethod,
        Range range)
    {
        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return {};

        Artist::FindParameters params;
        params.setStarringUser(userId, *scrobbler);
        params.setClusters(clusterIds);
        params.setLinkType(linkType);
        params.setSortMethod(sortMethod);
        params.setRange(range);

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        return Artist::find(session, params);
    }

    void ScrobblingService::star(UserId userId, ReleaseId releaseId)
    {
        star<Release, ReleaseId, StarredRelease>(userId, releaseId);
    }

    void ScrobblingService::unstar(UserId userId, ReleaseId releaseId)
    {
        unstar<Release, ReleaseId, StarredRelease>(userId, releaseId);
    }

    bool ScrobblingService::isStarred(UserId userId, ReleaseId releaseId)
    {
        return isStarred<Release, ReleaseId, StarredRelease>(userId, releaseId);
    }

    Wt::WDateTime ScrobblingService::getStarredDateTime(UserId userId, ReleaseId releaseId)
    {
        return getStarredDateTime<Release, ReleaseId, StarredRelease>(userId, releaseId);
    }

    ScrobblingService::ReleaseContainer ScrobblingService::getStarredReleases(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return {};

        Release::FindParameters params;
        params.setStarringUser(userId, *scrobbler);
        params.setClusters(clusterIds);
        params.setSortMethod(ReleaseSortMethod::StarredDateDesc);
        params.setRange(range);

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        return Release::find(session, params);
    }

    void ScrobblingService::star(UserId userId, TrackId trackId)
    {
        star<Track, TrackId, StarredTrack>(userId, trackId);
    }

    void ScrobblingService::unstar(UserId userId, TrackId trackId)
    {
        unstar<Track, TrackId, StarredTrack>(userId, trackId);
    }

    bool ScrobblingService::isStarred(UserId userId, TrackId trackId)
    {
        return isStarred<Track, TrackId, StarredTrack>(userId, trackId);
    }

    Wt::WDateTime ScrobblingService::getStarredDateTime(UserId userId, TrackId trackId)
    {
        return getStarredDateTime<Track, TrackId, StarredTrack>(userId, trackId);
    }

    ScrobblingService::TrackContainer ScrobblingService::getStarredTracks(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        auto scrobbler{ getUserScrobbler(userId) };
        if (!scrobbler)
            return {};

        Track::FindParameters params;
        params.setStarringUser(userId, *scrobbler);
        params.setClusters(clusterIds);
        params.setSortMethod(TrackSortMethod::StarredDateDesc);
        params.setRange(range);

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createSharedTransaction() };

        return Track::find(session, params);
    }
} // ns Scrobbling

