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

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/post.hpp>

#include "core/IConfig.hpp"
#include "core/Service.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/User.hpp"
#include "services/scrobbling/Exception.hpp"

#include "Utils.hpp"

namespace lms::scrobbling::lastFm
{
    namespace
    {
        constexpr std::size_t maxBatchSize{ 50 };

        struct TrackInfo
        {
            std::string artistName;
            std::string trackName;
            std::optional<std::string> albumName;
            std::optional<std::chrono::seconds> duration;
        };

        std::optional<TrackInfo> getTrackInfo(db::Session& session, const scrobbling::Listen& listen)
        {
            auto transaction{ session.createReadTransaction() };

            const db::Track::pointer track{ db::Track::find(session, listen.trackId) };
            if (!track)
                return std::nullopt;

            const std::string artistName{ track->getArtistDisplayName() };
            if (artistName.empty())
            {
                LOG(DEBUG, "Track '" << track->getAbsoluteFilePath() << "' cannot be scrobbled: no artist name");
                return std::nullopt;
            }

            TrackInfo info;
            info.artistName = artistName;
            info.trackName = track->getName();

            if (const auto release{ track->getRelease() })
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
        , _submitPeriod{ core::Service<core::IConfig>::get()->getULong("lastfm-submit-period-hours", 1) }
        , _client{ client }
    {
        LOG(INFO, "Starting Last.fm scrobblings synchronizer, submit period = " << _submitPeriod.count() << " hours");
        if (_submitPeriod.count() > 0)
            scheduleSubmit(std::chrono::seconds{ 30 });
    }

    ScrobblingsSynchronizer::~ScrobblingsSynchronizer() = default;

    void ScrobblingsSynchronizer::enqueListen(const TimedListen& listen)
    {
        assert(listen.listenedAt.isValid());
        enqueListen(listen, listen.listenedAt);
    }

    void ScrobblingsSynchronizer::enqueListenNow(const scrobbling::Listen& listen)
    {
        enqueListen(listen, {});
    }

    void ScrobblingsSynchronizer::enqueListen(const scrobbling::Listen& listen, const Wt::WDateTime& timePoint)
    {
        const utils::LastFmCredentials creds{ utils::getLastFmCredentials(_db.getTLSSession(), listen.userId) };
        if (creds.apiKey.empty() || creds.apiSecret.empty() || creds.sessionKey.empty())
        {
            LOG(DEBUG, "Missing Last.fm credentials for user, skipping");
            return;
        }

        const std::optional<TrackInfo> info{ getTrackInfo(_db.getTLSSession(), listen) };
        if (!info)
        {
            LOG(DEBUG, "Cannot build scrobble params: skipping");
            return;
        }

        std::map<std::string, std::string> params{ timePoint.isValid() ? buildScrobbleParams(*info, timePoint, 0) : buildNowPlayingParams(*info) };

        params.emplace("api_key", creds.apiKey);
        params.emplace("sk", creds.sessionKey);
        params.emplace("format", "json");
        params["api_sig"] = utils::computeApiSig(params, creds.apiSecret);

        core::http::ClientPOSTRequestParameters request;
        request.relativeUrl = "/2.0/";

        if (timePoint.isValid())
        {
            const TimedListen timedListen{ listen, timePoint };
            saveListen(timedListen, db::SyncState::PendingAdd);

            request.priority = core::http::ClientRequestParameters::Priority::Normal;
            request.onSuccessFunc = [this, timedListen](const Wt::Http::Message&) {
                boost::asio::post(boost::asio::bind_executor(_strand, [this, timedListen] {
                    saveListen(timedListen, db::SyncState::Synchronized);
                }));
            };
        }
        else
        {
            request.priority = core::http::ClientRequestParameters::Priority::High;
            // "now playing" is fire-and-forget, no retry
        }

        request.message.addBodyText(utils::buildFormBody(params));
        request.message.addHeader("Content-Type", "application/x-www-form-urlencoded");
        _client.sendPOSTRequest(std::move(request));
    }

    bool ScrobblingsSynchronizer::saveListen(const TimedListen& listen, db::SyncState syncState)
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        db::Listen::pointer dbListen{ db::Listen::find(session, listen.userId, listen.trackId, db::ScrobblingBackend::LastFm, listen.listenedAt) };
        if (!dbListen)
        {
            const db::User::pointer user{ db::User::find(session, listen.userId) };
            if (!user)
                return false;

            const db::Track::pointer track{ db::Track::find(session, listen.trackId) };
            if (!track)
                return false;

            dbListen = session.create<db::Listen>(user, track, db::ScrobblingBackend::LastFm, listen.listenedAt);
            dbListen.modify()->setSyncState(syncState);
            return true;
        }

        if (dbListen->getSyncState() == syncState)
            return false;

        dbListen.modify()->setSyncState(syncState);
        return true;
    }

    void ScrobblingsSynchronizer::enquePendingListens()
    {
        std::map<db::UserId, std::vector<TimedListen>> pendingByUser;

        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::Listen::FindParameters params;
            params.setScrobblingBackend(db::ScrobblingBackend::LastFm)
                .setSyncState(db::SyncState::PendingAdd)
                .setRange(db::Range{ 0, maxBatchSize * 10 });

            const db::RangeResults results{ db::Listen::find(session, params) };
            for (const db::ListenId listenId : results.results)
            {
                const db::Listen::pointer dbListen{ db::Listen::find(session, listenId) };

                TimedListen tl;
                tl.listenedAt = dbListen->getDateTime();
                tl.userId = dbListen->getUser()->getId();
                tl.trackId = dbListen->getTrack()->getId();
                pendingByUser[tl.userId].push_back(tl);
            }
        }

        for (auto& [userId, listens] : pendingByUser)
        {
            const utils::LastFmCredentials creds{ utils::getLastFmCredentials(_db.getTLSSession(), userId) };
            if (creds.apiKey.empty() || creds.apiSecret.empty() || creds.sessionKey.empty())
            {
                LOG(DEBUG, "Missing Last.fm credentials for user, skipping");
                continue;
            }

            for (std::span<const TimedListen> remaining{ listens }; !remaining.empty();)
            {
                const std::size_t count{ std::min(maxBatchSize, remaining.size()) };
                sendScrobbleBatch(creds, remaining.first(count));
                remaining = remaining.subspan(count);
            }
        }
    }

    void ScrobblingsSynchronizer::sendScrobbleBatch(const utils::LastFmCredentials& creds, std::span<const TimedListen> listens)
    {
        std::map<std::string, std::string> params;
        params["method"] = "track.scrobble";

        std::vector<TimedListen> validListens;
        db::Session& session{ _db.getTLSSession() };

        for (const TimedListen& listen : listens)
        {
            const std::optional<TrackInfo> info{ getTrackInfo(session, listen) };
            if (!info)
                continue;

            auto trackParams{ buildScrobbleParams(*info, listen.listenedAt, validListens.size()) };
            trackParams.erase("method");
            params.merge(std::move(trackParams));
            validListens.push_back(listen);
        }

        if (validListens.empty())
            return;

        LOG(DEBUG, "Sending scrobble batch of " << validListens.size() << " listens");

        params["api_key"] = creds.apiKey;
        params["sk"] = creds.sessionKey;
        params["format"] = "json";
        params["api_sig"] = utils::computeApiSig(params, creds.apiSecret);

        core::http::ClientPOSTRequestParameters request;
        request.relativeUrl = "/2.0/";
        request.priority = core::http::ClientRequestParameters::Priority::Normal;
        request.message.addBodyText(utils::buildFormBody(params));
        request.message.addHeader("Content-Type", "application/x-www-form-urlencoded");
        request.onSuccessFunc = [this, validListens](const Wt::Http::Message&) {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, validListens] {
                for (const TimedListen& listen : validListens)
                    saveListen(listen, db::SyncState::Synchronized);
            }));
        };

        _client.sendPOSTRequest(std::move(request));
    }

    void ScrobblingsSynchronizer::scheduleSubmit(std::chrono::seconds fromNow)
    {
        LOG(DEBUG, "Scheduled pending retry in " << fromNow.count() << " seconds");
        _submitTimer.expires_after(fromNow);
        _submitTimer.async_wait(boost::asio::bind_executor(_strand, [this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
                return;
            if (ec)
                throw Exception{ "Last.fm retry timer failure: " + std::string{ ec.message() } };

            if (_submitPeriod.count() > 0)
            {
                enquePendingListens();
                scheduleSubmit(_submitPeriod);
            }
        }));
    }
} // namespace lms::scrobbling::lastFm
