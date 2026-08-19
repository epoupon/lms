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

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/ListenBackendSync.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"

#include "lastfm/LastFmBackend.hpp"
#include "listenbrainz/ListenBrainzBackend.hpp"

namespace lms::scrobbling
{
    using namespace db;

    std::unique_ptr<IScrobblingService> createScrobblingService(boost::asio::io_context& ioContext, db::IDb& db)
    {
        return std::make_unique<ScrobblingService>(ioContext, db);
    }

    ScrobblingService::ScrobblingService(boost::asio::io_context& ioContext, db::IDb& db)
        : _db{ db }
    {
        LMS_LOG(SCROBBLING, INFO, "Starting service...");
        {
            auto backend{ std::make_unique<listenBrainz::ListenBrainzBackend>(ioContext, _db) };
            _listenBrainzBackend = backend.get();
            _scrobblingBackends.emplace(ScrobblingBackend::ListenBrainz, std::move(backend));
        }
        {
            auto backend{ std::make_unique<lastFm::LastFmBackend>(ioContext, _db) };
            _lastFmBackend = backend.get();
            _scrobblingBackends.emplace(ScrobblingBackend::LastFm, std::move(backend));
        }
        LMS_LOG(SCROBBLING, INFO, "Service started!");
    }

    ScrobblingService::~ScrobblingService()
    {
        LMS_LOG(SCROBBLING, INFO, "Service stopped!");
    }

    void ScrobblingService::listenStarted(const Listen& listen)
    {
        insertNowPlayingEntry(listen);

        for (const ScrobblingBackend backend : getUserEnabledBackends(listen.userId))
            _scrobblingBackends[backend]->listenStarted(listen);
    }

    void ScrobblingService::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
    {
        const std::optional<TimedListen> recordedListen{ recordListen(listen, Wt::WDateTime::currentDateTime(), duration) };
        if (!recordedListen)
            return;

        for (const ScrobblingBackend backend : getUserEnabledBackends(listen.userId))
        {
            if (_scrobblingBackends[backend]->canBeScrobbled(listen.trackId, duration))
                _scrobblingBackends[backend]->listenFinished(*recordedListen, duration);
        }
    }

    void ScrobblingService::addTimedListen(const TimedListen& listen)
    {
        const std::optional<TimedListen> recordedListen{ recordListen(listen, listen.listenedAt, std::nullopt) };
        if (!recordedListen)
            return;

        for (const ScrobblingBackend backend : getUserEnabledBackends(listen.userId))
        {
            if (_scrobblingBackends[backend]->canBeScrobbled(listen.trackId, std::nullopt))
                _scrobblingBackends[backend]->addTimedListen(*recordedListen);
        }
    }

    std::optional<TimedListen> ScrobblingService::recordListen(const Listen& listen, const Wt::WDateTime& listenedAt, std::optional<std::chrono::seconds> duration)
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        const Track::pointer track{ Track::find(session, listen.trackId) };
        if (!track)
            return std::nullopt;

        // This must be less restrictive than the least restrictive backend
        const std::chrono::seconds minRequiredDuration{ std::min(std::chrono::seconds{ 5 }, std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()) / 2) };
        if (duration && *duration < minRequiredDuration)
            return std::nullopt;

        if (!db::Listen::find(session, listen.userId, listen.trackId, listenedAt))
        {
            const User::pointer user{ User::find(session, listen.userId) };
            if (!user)
                return std::nullopt;

            session.create<db::Listen>(user, track, listenedAt);
        }

        return TimedListen{ listen, listenedAt };
    }

    void ScrobblingService::initiateLastFmLink(db::UserId userId,
                                               std::string_view apiKey,
                                               std::string_view apiSecret,
                                               std::function<void(std::string_view authUrl)> onSuccess,
                                               std::function<void()> onFailure)
    {
        if (_lastFmBackend)
            _lastFmBackend->initiateLastFmLink(userId, apiKey, apiSecret, std::move(onSuccess), std::move(onFailure));
        else
            onFailure();
    }

    void ScrobblingService::continueLastFmLink(db::UserId userId,
                                               std::function<void()> onSuccess,
                                               std::function<void()> onFailure)
    {
        if (_lastFmBackend)
            _lastFmBackend->continueLastFmLink(userId, std::move(onSuccess), std::move(onFailure));
        else
            onFailure();
    }

    void ScrobblingService::visitNowPlayingListens(const std::function<void(Clock::time_point startedAt, const Listen&)>& visitor, db::UserId userId)
    {
        const Clock::time_point now{ Clock::now() };

        std::shared_lock lock{ _nowPlayingEntriesMutex };

        for (const auto& [entryUserId, entry] : _nowPlayingEntries)
        {
            if (userId.isValid() && entryUserId != userId)
                continue;

            if (entry.expiryAt <= now)
                continue;

            visitor(entry.startedAt, Listen{ .userId = entryUserId, .trackId = entry.trackId });
        }
    }

    core::EnumSet<ScrobblingBackend> ScrobblingService::getUserEnabledBackends(UserId userId)
    {
        core::EnumSet<ScrobblingBackend> backends;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };
        if (const User::pointer user{ User::find(session, userId) })
            backends = user->getScrobblingBackends();

        return backends;
    }

    void ScrobblingService::requestImmediateImport(db::UserId userId, db::ScrobblingBackend backend)
    {
        if (!getUserEnabledBackends(userId).contains(backend))
            return;

        _scrobblingBackends[backend]->requestImmediateImport(userId);
    }

    void ScrobblingService::requestImmediateExport(db::UserId userId, db::ScrobblingBackend backend)
    {
        if (!getUserEnabledBackends(userId).contains(backend))
            return;

        markPendingExports(userId, backend);
        _scrobblingBackends[backend]->requestImmediateExport();
    }

    void ScrobblingService::markPendingExports(db::UserId userId, db::ScrobblingBackend backend)
    {
        IScrobblingBackend& backendImpl{ *_scrobblingBackends[backend] };

        constexpr std::size_t chunkSize{ 500 };
        for (std::size_t offset{};; offset += chunkSize)
        {
            std::vector<db::ListenId> ids;
            {
                Session& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::Listen::FindParameters params;
                params.setUser(userId).setRange(db::Range{ offset, chunkSize });
                ids = db::Listen::find(session, params);
            }

            if (ids.empty())
                break;

            {
                Session& session{ _db.getTLSSession() };
                auto transaction{ session.createWriteTransaction() };

                for (const db::ListenId id : ids)
                {
                    if (db::ListenBackendSync::find(session, id, backend))
                        continue; // already pending or synchronized: leave untouched

                    const db::Listen::pointer listen{ db::Listen::find(session, id) };
                    if (!listen || !backendImpl.canBeScrobbled(listen->getTrack()->getId(), std::nullopt))
                        continue;

                    session.create<db::ListenBackendSync>(listen, backend);
                }
            }

            if (ids.size() < chunkSize)
                break;
        }
    }

    void ScrobblingService::insertNowPlayingEntry(const Listen& listen)
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        if (const db::Track::pointer track{ db::Track::find(session, listen.trackId) })
        {
            const Clock::time_point now{ Clock::now() };

            std::unique_lock lock{ _nowPlayingEntriesMutex };

            // Add an extra delay to ensure the listen is not purged too early
            _nowPlayingEntries.insert_or_assign(listen.userId, NowPlayingEntry{ .startedAt = now, .expiryAt = now + track->getDuration() + std::chrono::seconds{ 5 }, .trackId = listen.trackId });
        }
    }
} // namespace lms::scrobbling
