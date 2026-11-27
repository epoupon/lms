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

#include "ScrobblingsSynchronizer.hpp"

#include <algorithm>
#include <array>
#include <chrono>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/post.hpp>

#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/ListenBackendSync.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/User.hpp"

#include "Utils.hpp"

namespace lms::scrobbling::lastFm
{
    namespace
    {
        constexpr std::size_t maxBatchSize{ 50 };

        // 1, 2, 4 ... 64 minutes
        constexpr std::size_t maxExportRetryDoublings{ 6 };
        std::chrono::minutes getExportRetryDelay(std::size_t consecutiveFailures)
        {
            return std::chrono::minutes{ 1 << std::min(consecutiveFailures, maxExportRetryDoublings) };
        }

        struct TrackInfo
        {
            std::string artistName;
            std::string trackName;
            std::optional<std::string> albumName;
            std::optional<std::chrono::seconds> duration;
        };

        std::optional<TrackInfo> getTrackInfo(db::ISession& session, const scrobbling::Listen& listen)
        {
            auto transaction{ session.createReadTransaction() };

            const std::optional<db::Track> track{ db::Track::find(session, listen.trackId) };
            if (!track)
                return std::nullopt;

            const std::string artistName{ track->getArtistDisplayName() };
            if (artistName.empty())
            {
                LMS_LOG_LASTFM(DEBUG, "Track '" << track->getAbsoluteFilePath() << "' cannot be scrobbled: no artist name");
                return std::nullopt;
            }

            TrackInfo info;
            info.artistName = artistName;
            info.trackName = track->getName();

            if (const auto release{ db::Release::find(session, track->getReleaseId()) })
                info.albumName = release->getName();

            const auto secs{ std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()) };
            if (secs.count() > 0)
                info.duration = secs;

            return info;
        }

        std::map<std::string, std::string> buildScrobbleParams(const TrackInfo& info, const Wt::WDateTime& timePoint, std::size_t index)
        {
            const std::string indexStr{ "[" + std::to_string(index) + "]" };

            std::map<std::string, std::string> params;
            params["method"] = "track.scrobble";
            params["artist" + indexStr] = info.artistName;
            params["track" + indexStr] = info.trackName;
            if (info.albumName)
                params["album" + indexStr] = *info.albumName;
            if (info.duration)
                params["duration" + indexStr] = std::to_string(info.duration->count());
            params["timestamp" + indexStr] = std::to_string(timePoint.toTime_t());

            return params;
        }

        std::map<std::string, std::string> buildNowPlayingParams(const TrackInfo& info)
        {
            std::map<std::string, std::string> params;
            params["method"] = "track.updateNowPlaying";
            params["artist"] = info.artistName;
            params["track"] = info.trackName;
            if (info.albumName)
                params["album"] = *info.albumName;
            if (info.duration)
                params["duration"] = std::to_string(info.duration->count());

            return params;
        }

    } // namespace

    ScrobblingsSynchronizer::ScrobblingsSynchronizer(boost::asio::io_context& ioContext, db::IDb& db, core::http::IClient& client)
        : _ioContext{ ioContext }
        , _db{ db }
        , _client{ client }
    {
        LMS_LOG_LASTFM(INFO, "Starting Last.fm scrobblings synchronizer");

        // resume the exports interrupted by the last shutdown
        boost::asio::post(_ioContext, [this] { requestExportForEnabledUsers(); });
    }

    ScrobblingsSynchronizer::~ScrobblingsSynchronizer() = default;

    void ScrobblingsSynchronizer::enqueListen(const TimedListen& listen)
    {
        assert(listen.listenedAt.isValid());

        if (saveListen(listen, db::SyncState::PendingAdd))
        {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, userId = listen.userId] {
                startExport(getExportState(userId));
            }));
        }
    }

    void ScrobblingsSynchronizer::enqueListenNow(const scrobbling::Listen& listen)
    {
        const std::optional<TrackInfo> info{ getTrackInfo(_db.getTLSSession(), listen) };
        if (!info)
            return;

        const utils::LastFmCredentials creds{ utils::getLastFmCredentials(_db.getTLSSession(), listen.userId) };
        if (creds.apiKey.empty() || creds.apiSecret.empty() || creds.sessionKey.empty())
            return;

        std::map<std::string, std::string> params{ buildNowPlayingParams(*info) };
        params.emplace("api_key", creds.apiKey);
        params.emplace("sk", creds.sessionKey);
        params.emplace("format", "json");
        params["api_sig"] = utils::computeApiSig(params, creds.apiSecret);

        // fire and forget: a stale "now playing" is not worth a retry
        core::http::ClientPOSTRequestParameters request;
        request.relativeUrl = "/2.0/";
        request.priority = core::http::ClientRequestParameters::Priority::High;
        request.message.addBodyText(utils::buildFormBody(params));
        request.message.addHeader("Content-Type", "application/x-www-form-urlencoded");
        _client.sendPOSTRequest(std::move(request));
    }

    void ScrobblingsSynchronizer::requestImmediateExport(db::UserId userId)
    {
        boost::asio::post(boost::asio::bind_executor(_strand, [this, userId] {
            ExportState& exportState{ getExportState(userId) };
            // asked for by the user (backend turned on, account linked): no reason to wait for the end of a retry delay
            if (exportState.retryTimer.cancel() > 0)
            {
                sendNextScrobbleBatch(exportState);
                return;
            }

            startExport(exportState);
        }));
    }

    void ScrobblingsSynchronizer::startExport(ExportState& exportState)
    {
        assert(_strand.running_in_this_thread());

        // the running export picks up what was queued meanwhile, and a pending retry keeps its delay
        if (exportState.exporting)
            return;

        exportState.exporting = true;
        sendNextScrobbleBatch(exportState);
    }

    ScrobblingsSynchronizer::ExportState& ScrobblingsSynchronizer::getExportState(db::UserId userId)
    {
        assert(_strand.running_in_this_thread());

        return _exportStates.try_emplace(userId, userId, _ioContext).first->second;
    }

    void ScrobblingsSynchronizer::requestExportForEnabledUsers()
    {
        std::vector<db::UserId> userIds;
        {
            db::ISession& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::User::find(session, db::User::FindParameters{}, [&](const db::User& user) {
                if (user.getScrobblingBackends().contains(db::ScrobblingBackend::LastFm))
                    userIds.push_back(user.getId());
            });
        }

        for (const db::UserId userId : userIds)
            requestImmediateExport(userId);
    }

    bool ScrobblingsSynchronizer::saveListen(const TimedListen& listen, db::SyncState syncState)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        std::optional<db::Listen> dbListen{ db::Listen::find(session, listen.userId, listen.trackId, listen.listenedAt) };
        if (!dbListen)
        {
            const std::optional<db::User> user{ db::User::find(session, listen.userId) };
            if (!user)
                return false;

            const std::optional<db::Track> track{ db::Track::find(session, listen.trackId) };
            if (!track)
                return false;

            dbListen = db::Listen::create(session, user->getId(), track->getId(), listen.listenedAt);
        }

        std::optional<db::ListenBackendSync> sync{ db::ListenBackendSync::find(session, dbListen->getId(), db::ScrobblingBackend::LastFm) };
        if (!sync)
            sync = db::ListenBackendSync::create(session, dbListen->getId(), db::ScrobblingBackend::LastFm);
        else if (sync->getSyncState() == syncState)
            return false;

        sync->setSyncState(syncState);
        db::ListenBackendSync::update(session, *sync);
        return true;
    }

    bool ScrobblingsSynchronizer::handleScrobbleResult(db::ListenBackendSyncId syncId, const utils::ScrobbleResult& result)
    {
        const std::string detail{ result.ignoredMessage.empty() ? std::string{} : (": " + result.ignoredMessage) };

        switch (result.ignoredCode)
        {
        case utils::ScrobbleIgnoredCode::None:
            markListenSynchronized(syncId);
            return true;

        case utils::ScrobbleIgnoredCode::DailyLimitExceeded:
            LMS_LOG_LASTFM(WARNING, "Scrobble deferred, will retry later" << detail);
            return false;

        case utils::ScrobbleIgnoredCode::ArtistIgnored:
        case utils::ScrobbleIgnoredCode::TrackIgnored:
        case utils::ScrobbleIgnoredCode::TimestampTooOld:
        case utils::ScrobbleIgnoredCode::TimestampTooNew:
            break;
        }

        LMS_LOG_LASTFM(WARNING, "Scrobble ignored by Last.fm: " << utils::toString(result.ignoredCode) << detail);
        markListenSynchronized(syncId);
        return true;
    }

    void ScrobblingsSynchronizer::dropListen(db::ListenBackendSyncId syncId)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        db::ListenBackendSync::destroy(session, std::array{ syncId });
    }

    void ScrobblingsSynchronizer::markListenSynchronized(db::ListenBackendSyncId syncId)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        // gone if the backend was turned off while the listen was being sent
        std::optional<db::ListenBackendSync> sync{ db::ListenBackendSync::find(session, syncId) };
        if (!sync)
            return;

        sync->setSyncState(db::SyncState::Synchronized);
        db::ListenBackendSync::update(session, *sync);
    }

    void ScrobblingsSynchronizer::sendNextScrobbleBatch(ExportState& exportState)
    {
        assert(_strand.running_in_this_thread());

        // not linked yet: the queue is sent once the user links their account
        const utils::LastFmCredentials creds{ utils::getLastFmCredentials(_db.getTLSSession(), exportState.userId) };
        if (creds.apiKey.empty() || creds.apiSecret.empty() || creds.sessionKey.empty())
        {
            exportState.exporting = false;
            return;
        }

        std::map<std::string, std::string> params;
        params["method"] = "track.scrobble";

        // sync rows of the listens in the batch, in the order Last.fm reports their results
        std::vector<db::ListenBackendSyncId> batchSyncIds;
        while (batchSyncIds.empty())
        {
            std::vector<std::pair<db::ListenBackendSyncId, TimedListen>> pendingListens;
            {
                db::ISession& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::ListenBackendSync::FindParameters findParams;
                findParams.setUser(exportState.userId)
                    .setBackend(db::ScrobblingBackend::LastFm)
                    .setSyncState(db::SyncState::PendingAdd)
                    .setRange(db::Range{ 0, maxBatchSize });

                db::ListenBackendSync::find(session, findParams, [&](const db::ListenBackendSync& sync) {
                    const std::optional<db::Listen> listen{ db::Listen::find(session, sync.getListenId()) };
                    pendingListens.emplace_back(sync.getId(), TimedListen{ { listen->getUserId(), listen->getTrackId() }, listen->getDateTime() });
                });
            }

            if (pendingListens.empty())
            {
                exportState.exporting = false;
                return;
            }

            for (const auto& [syncId, listen] : pendingListens)
            {
                const std::optional<TrackInfo> info{ getTrackInfo(_db.getTLSSession(), listen) };
                if (!info)
                {
                    LMS_LOG_LASTFM(DEBUG, "Listen cannot be scrobbled (no artist tag): dropping it");
                    dropListen(syncId);
                    continue;
                }

                auto trackParams{ buildScrobbleParams(*info, listen.listenedAt, batchSyncIds.size()) };
                trackParams.erase("method");
                params.merge(std::move(trackParams));
                batchSyncIds.push_back(syncId);
            }
        }

        LMS_LOG_LASTFM(DEBUG, "Sending scrobble batch of " << batchSyncIds.size() << " listens");

        params["api_key"] = creds.apiKey;
        params["sk"] = creds.sessionKey;
        params["format"] = "json";
        params["api_sig"] = utils::computeApiSig(params, creds.apiSecret);

        core::http::ClientPOSTRequestParameters request;
        request.relativeUrl = "/2.0/";
        request.priority = core::http::ClientRequestParameters::Priority::Normal;
        request.message.addBodyText(utils::buildFormBody(params));
        request.message.addHeader("Content-Type", "application/x-www-form-urlencoded");
        request.onSuccessFunc = [this, batchSyncIds, &exportState](const Wt::Http::Message& msg) {
            const auto results{ utils::parseScrobbleResults(msg.body(), batchSyncIds.size()) };
            // likely not from Last.fm (proxy, captive portal...), which reports its errors with an HTTP error status
            if (results.size() != batchSyncIds.size())
                LMS_LOG_LASTFM(WARNING, "Cannot understand the response to a scrobble batch, will send it again: '" << std::string_view{ msg.body() }.substr(0, 200) << "'");

            boost::asio::post(boost::asio::bind_executor(_strand, [this, batchSyncIds, results, &exportState] {
                if (results.size() != batchSyncIds.size())
                {
                    scheduleExportRetry(exportState);
                    return;
                }

                bool allHandled{ true };
                for (std::size_t i{ 0 }; i < batchSyncIds.size(); ++i)
                {
                    if (!handleScrobbleResult(batchSyncIds[i], results[i]))
                        allHandled = false;
                }

                if (!allHandled)
                {
                    scheduleExportRetry(exportState);
                    return;
                }

                exportState.consecutiveFailures = 0;
                sendNextScrobbleBatch(exportState);
            }));
        };
        // Last.fm reports rejected scrobbles in a successful response: any failure is worth a retry
        request.onFailureFunc = [this, &exportState](std::optional<int> /*httpStatus*/) {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, &exportState] {
                scheduleExportRetry(exportState);
            }));
        };

        _client.sendPOSTRequest(std::move(request));
    }

    void ScrobblingsSynchronizer::scheduleExportRetry(ExportState& exportState)
    {
        assert(_strand.running_in_this_thread());

        const std::size_t failureCount{ exportState.consecutiveFailures++ };
        const std::chrono::minutes retryDelay{ getExportRetryDelay(failureCount) };
        if (failureCount == maxExportRetryDoublings)
            LMS_LOG_LASTFM(WARNING, "Still cannot send scrobbles for user " << exportState.userId.toString() << ", now retrying every " << retryDelay.count() << " minutes: is Last.fm down or the account link broken?");
        else
            LMS_LOG_LASTFM(INFO, "Cannot send scrobbles, retrying in " << retryDelay.count() << " minutes");

        exportState.retryTimer.expires_after(retryDelay);
        exportState.retryTimer.async_wait(boost::asio::bind_executor(_strand, [this, &exportState](const boost::system::error_code& ec) {
            if (ec)
                return; // cancelled on shutdown or by requestImmediateExport

            sendNextScrobbleBatch(exportState);
        }));
    }
} // namespace lms::scrobbling::lastFm
