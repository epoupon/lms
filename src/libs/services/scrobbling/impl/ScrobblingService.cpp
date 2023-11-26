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

#include "services/database/Artist.hpp"
#include "services/database/Db.hpp"
#include "services/database/Listen.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/ILogger.hpp"

#include "internal/InternalBackend.hpp"
#include "listenbrainz/ListenBrainzBackend.hpp"

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
        LMS_LOG(SCROBBLING, INFO, "Starting service...");
        _scrobblingBackends.emplace(ScrobblingBackend::Internal, std::make_unique<InternalBackend>(_db));
        _scrobblingBackends.emplace(ScrobblingBackend::ListenBrainz, std::make_unique<ListenBrainz::ListenBrainzBackend>(ioContext, _db));
        LMS_LOG(SCROBBLING, INFO, "Service started!");
    }

    ScrobblingService::~ScrobblingService()
    {
        LMS_LOG(SCROBBLING, INFO, "Service stopped!");
    }

    void ScrobblingService::listenStarted(const Listen& listen)
    {
        if (std::optional<ScrobblingBackend> backend{ getUserBackend(listen.userId) })
            _scrobblingBackends[*backend]->listenStarted(listen);
    }

    void ScrobblingService::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
    {
        if (std::optional<ScrobblingBackend> backend{ getUserBackend(listen.userId) })
            _scrobblingBackends[*backend]->listenFinished(listen, duration);
    }

    void ScrobblingService::addTimedListen(const TimedListen& listen)
    {
        if (std::optional<ScrobblingBackend> backend{ getUserBackend(listen.userId) })
            _scrobblingBackends[*backend]->addTimedListen(listen);
    }

    std::optional<ScrobblingBackend> ScrobblingService::getUserBackend(UserId userId)
    {
        std::optional<ScrobblingBackend> backend;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };
        if (const User::pointer user{ User::find(session, userId) })
            backend = user->getScrobblingBackend();

        return backend;
    }

    ScrobblingService::ArtistContainer ScrobblingService::getRecentArtists(UserId userId, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, Range range)
    {
        ArtistContainer res;

        const auto backend{ getUserBackend(userId) };
        if (!backend)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        res = Database::Listen::getRecentArtists(session, userId, *backend, clusterIds, linkType, range);
        return res;
    }

    ScrobblingService::ReleaseContainer ScrobblingService::getRecentReleases(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        ReleaseContainer res;

        const auto backend{ getUserBackend(userId) };
        if (!backend)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        res = Database::Listen::getRecentReleases(session, userId, *backend, clusterIds, range);
        return res;
    }

    ScrobblingService::TrackContainer ScrobblingService::getRecentTracks(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        TrackContainer res;

        const auto backend{ getUserBackend(userId) };
        if (!backend)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        res = Database::Listen::getRecentTracks(session, userId, *backend, clusterIds, range);
        return res;
    }

    std::size_t ScrobblingService::getCount(Database::UserId userId, Database::ReleaseId releaseId)
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };
        return Database::Listen::getCount(session, userId, releaseId);
    }

    std::size_t ScrobblingService::getCount(Database::UserId userId, Database::TrackId trackId)
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };
        return Database::Listen::getCount(session, userId, trackId);
    }

    Wt::WDateTime ScrobblingService::getLastListenDateTime(Database::UserId userId, Database::ReleaseId releaseId)
    {
        const auto backend{ getUserBackend(userId) };
        if (!backend)
            return {};

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const Database::Listen::pointer listen{ Database::Listen::getMostRecentListen(session, userId, *backend, releaseId) };
        return listen ? listen->getDateTime() : Wt::WDateTime{};
    }

    Wt::WDateTime ScrobblingService::getLastListenDateTime(Database::UserId userId, Database::TrackId trackId)
    {
        const auto backend{ getUserBackend(userId) };
        if (!backend)
            return {};

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const Database::Listen::pointer listen{ Database::Listen::getMostRecentListen(session, userId, *backend, trackId) };
        return listen ? listen->getDateTime() : Wt::WDateTime{};
    }

    // Top
    ScrobblingService::ArtistContainer ScrobblingService::getTopArtists(UserId userId, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, Range range)
    {
        ArtistContainer res;

        const auto backend{ getUserBackend(userId) };
        if (!backend)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        res = Database::Listen::getTopArtists(session, userId, *backend, clusterIds, linkType, range);
        return res;
    }

    ScrobblingService::ReleaseContainer ScrobblingService::getTopReleases(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        ReleaseContainer res;

        const auto backend{ getUserBackend(userId) };
        if (!backend)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        res = Database::Listen::getTopReleases(session, userId, *backend, clusterIds, range);
        return res;
    }

    ScrobblingService::TrackContainer ScrobblingService::getTopTracks(UserId userId, const std::vector<ClusterId>& clusterIds, Range range)
    {
        TrackContainer res;

        const auto backend{ getUserBackend(userId) };
        if (!backend)
            return res;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        res = Database::Listen::getTopTracks(session, userId, *backend, clusterIds, range);
        return res;
    }
} // ns Scrobbling

