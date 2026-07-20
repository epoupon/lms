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

#include "ListensSynchronizer.hpp"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Serializer.h>
#include <Wt/Json/Value.h>
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

#include "ListensParser.hpp"
#include "Utils.hpp"

namespace lms::scrobbling::listenBrainz
{
    namespace
    {
        struct Artist
        {
            std::string name;
            std::optional<core::UUID> mbid;
        };
        std::vector<Artist> getTrackArtists(const db::Track::pointer& track)
        {
            std::vector<Artist> artists;

            for (const db::TrackArtistLink::pointer& trackArtistLink : track->getArtistLinks(db::TrackArtistLinkType::Artist))
            {
                const auto trackArtist{ trackArtistLink->getArtist() };
                if (!trackArtist)
                    continue;

                artists.emplace_back(Artist{ std::string{ trackArtistLink->getArtistName() }, trackArtist->getMBID() });
            }

            return artists;
        }

        std::optional<Wt::Json::Object> listenToJsonPayload(db::Session& session, const scrobbling::Listen& listen, const Wt::WDateTime& timePoint)
        {
            auto transaction{ session.createReadTransaction() };

            const db::Track::pointer track{ db::Track::find(session, listen.trackId) };
            if (!track)
                return std::nullopt;

            const std::vector<Artist> artists{ getTrackArtists(track) };
            if (artists.empty())
            {
                LMS_LOG_LISTENBRAINZ(DEBUG, "Track cannot be scrobbled since it does not have any artist");
                return std::nullopt;
            }

            Wt::Json::Object additionalInfo;
            additionalInfo["listening_from"] = "LMS";
            additionalInfo["duration_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(track->getDuration()).count();
            if (const auto release{ track->getRelease() })
            {
                if (auto MBID{ release->getMBID() })
                    additionalInfo["release_mbid"] = Wt::Json::Value{ MBID->toString() };
                if (auto groupMBID{ release->getGroupMBID() })
                    additionalInfo["release_group_mbid"] = Wt::Json::Value{ groupMBID->toString() };
            }

            {
                Wt::Json::Array artistMBIDs;
                for (const Artist& artist : artists)
                {
                    if (artist.mbid)
                        artistMBIDs.push_back(Wt::Json::Value{ artist.mbid->toString() });
                }

                if (!artistMBIDs.empty())
                    additionalInfo["artist_mbids"] = std::move(artistMBIDs);
            }

            if (auto MBID{ track->getTrackMBID() })
                additionalInfo["track_mbid"] = Wt::Json::Value{ MBID->toString() };

            if (auto MBID{ track->getRecordingMBID() })
                additionalInfo["recording_mbid"] = Wt::Json::Value{ MBID->toString() };

            if (const std::optional<std::size_t> trackNumber{ track->getTrackNumber() })
                additionalInfo["tracknumber"] = Wt::Json::Value{ static_cast<long long int>(*trackNumber) };

            Wt::Json::Object trackMetadata;
            trackMetadata["additional_info"] = std::move(additionalInfo);
            trackMetadata["artist_name"] = Wt::Json::Value{ std::string{ track->getArtistDisplayName() } };
            trackMetadata["track_name"] = Wt::Json::Value{ std::string{ track->getName() } };
            if (track->getRelease())
                trackMetadata["release_name"] = Wt::Json::Value{ std::string{ track->getRelease()->getName() } };

            Wt::Json::Object payload;
            payload["track_metadata"] = std::move(trackMetadata);
            if (timePoint.isValid())
                payload["listened_at"] = Wt::Json::Value{ static_cast<long long int>(timePoint.toTime_t()) };

            return payload;
        }

        std::string listenToJsonString(db::Session& session, const scrobbling::Listen& listen, const Wt::WDateTime& timePoint, std::string_view listenType)
        {
            std::string res;

            std::optional<Wt::Json::Object> payload{ listenToJsonPayload(session, listen, timePoint) };
            if (!payload)
                return res;

            Wt::Json::Object root;
            root["listen_type"] = Wt::Json::Value{ std::string{ listenType } };
            root["payload"] = Wt::Json::Array{ std::move(*payload) };

            res = Wt::Json::serialize(root);
            return res;
        }

        std::optional<std::size_t> parseListenCount(std::string_view msgBody)
        {
            try
            {
                Wt::Json::Object root;
                Wt::Json::parse(std::string{ msgBody }, root);

                const Wt::Json::Object& payload{ static_cast<const Wt::Json::Object&>(root.get("payload")) };
                return static_cast<int>(payload.get("count"));
            }
            catch (const Wt::WException& e)
            {
                LMS_LOG_LISTENBRAINZ(ERROR, "Cannot parse listen count response: " << e.what());
                return std::nullopt;
            }
        }

        db::TrackId tryGetMatchingTrack(db::Session& session, const Listen& listen)
        {
            using namespace db;

            auto transaction{ session.createReadTransaction() };

            // first try to match using track MBID, and then fallback on possibly ambiguous info
            if (listen.trackMBID)
            {
                const auto tracks{ Track::findByMBID(session, *listen.trackMBID) };
                // if duplicated files, do not record it (let the user correct its database)
                if (tracks.size() == 1)
                {
                    LMS_LOG_LISTENBRAINZ(DEBUG, "Matched listen '" << listen << "' using track MBID");
                    return tracks.front()->getId();
                }
                else if (tracks.size() > 1)
                {
                    LMS_LOG_LISTENBRAINZ(DEBUG, "Too many matches for listen '" << listen << "' using track MBID!");
                    return {};
                }
            }

            if (listen.recordingMBID)
            {
                const auto tracks{ Track::findByRecordingMBID(session, *listen.recordingMBID) };
                // if duplicated files, do not record it (let the user correct its database)
                if (tracks.size() == 1)
                {
                    LMS_LOG_LISTENBRAINZ(DEBUG, "Matched listen '" << listen << "' using recording MBID");
                    return tracks.front()->getId();
                }
                else if (tracks.size() > 1)
                {
                    LMS_LOG_LISTENBRAINZ(DEBUG, "Too many matches for listen '" << listen << "' using recording MBID!");
                    return {};
                }
            }

            assert(!listen.trackName.empty() && !listen.artistName.empty());

            // TODO check release MBID?
            Track::FindParameters params;
            params.setName(listen.trackName);
            params.setReleaseName(listen.releaseName);
            params.setArtistName(listen.artistName);
            if (listen.trackNumber)
                params.setTrackNumber(*listen.trackNumber);

            const auto tracks{ Track::findIds(session, params) };
            // conservative behavior: in case of multiple matches: reject
            if (tracks.size() == 1)
            {
                LMS_LOG_LISTENBRAINZ(DEBUG, "Matched listen '" << listen << "' using metadata");
                return tracks.front();
            }
            else if (tracks.size() > 1)
            {
                LMS_LOG_LISTENBRAINZ(DEBUG, "Too many matches for listen '" << listen << "' using metadata");
                return {};
            }

            LMS_LOG_LISTENBRAINZ(DEBUG, "No match for listen '" << listen << "'");
            return {};
        }
    } // namespace

    ListensSynchronizer::ListensSynchronizer(boost::asio::io_context& ioContext, db::IDb& db, core::http::IClient& client)
        : _ioContext{ ioContext }
        , _db{ db }
        , _client{ client }
        , _maxSyncListenCount{ core::Service<core::IConfig>::get()->getULong("listenbrainz-max-sync-listen-count", 1000) }
        , _syncListensPeriod{ core::Service<core::IConfig>::get()->getULong("listenbrainz-sync-listens-period-hours", 1) }
    {
        LMS_LOG_LISTENBRAINZ(INFO, "Starting Listens synchronizer, maxSyncListenCount = " << _maxSyncListenCount << ", _syncListensPeriod = " << _syncListensPeriod.count() << " hours");

        scheduleSync(std::chrono::seconds{ 30 });
    }

    void ListensSynchronizer::enqueListen(const TimedListen& listen)
    {
        assert(listen.listenedAt.isValid());
        enqueListen(listen, listen.listenedAt);
    }

    void ListensSynchronizer::enqueListenNow(const scrobbling::Listen& listen)
    {
        enqueListen(listen, {});
    }

    void ListensSynchronizer::enqueListen(const scrobbling::Listen& listen, const Wt::WDateTime& timePoint)
    {
        core::http::ClientPOSTRequestParameters request;
        request.relativeUrl = "/1/submit-listens";

        if (timePoint.isValid())
        {
            const TimedListen timedListen{ listen, timePoint };
            // We want the listen to be sent again later in case of failure, so we just save it as pending send
            saveListen(timedListen, db::SyncState::PendingAdd);

            request.priority = core::http::ClientRequestParameters::Priority::Normal;
            request.onSuccessFunc = [this, timedListen](const Wt::Http::Message&) {
                boost::asio::post(boost::asio::bind_executor(_strand, [this, timedListen] {
                    if (saveListen(timedListen, db::SyncState::Synchronized))
                    {
                        UserContext& context{ getUserContext(timedListen.userId) };
                        if (context.listenCount)
                            (*context.listenCount)++;
                    }
                }));
            };
            // on failure, this listen will be sent during the next sync
        }
        else
        {
            // We want "listen now" to appear as soon as possible
            request.priority = core::http::ClientRequestParameters::Priority::High;
            // don't retry on failure
        }

        std::string bodyText{ listenToJsonString(_db.getTLSSession(), listen, timePoint, timePoint.isValid() ? "single" : "playing_now") };
        if (bodyText.empty())
        {
            LMS_LOG_LISTENBRAINZ(DEBUG, "Cannot convert listen to json: skipping");
            return;
        }

        const std::string listenBrainzToken{ utils::getListenBrainzToken(_db.getTLSSession(), listen.userId) };
        if (listenBrainzToken.empty())
        {
            LMS_LOG_LISTENBRAINZ(DEBUG, "No listenbrainz token found: skipping");
            return;
        }

        request.message.addBodyText(bodyText);
        request.message.addHeader("Authorization", "Token " + listenBrainzToken);
        request.message.addHeader("Content-Type", "application/json");
        _client.sendPOSTRequest(std::move(request));
    }

    bool ListensSynchronizer::saveListen(const TimedListen& listen, db::SyncState scrobblingState)
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() }; // TODO: unique only if needed

        db::Listen::pointer dbListen{ db::Listen::find(session, listen.userId, listen.trackId, db::ScrobblingBackend::ListenBrainz, listen.listenedAt) };
        if (!dbListen)
        {
            const db::User::pointer user{ db::User::find(session, listen.userId) };
            if (!user)
                return false;

            const db::Track::pointer track{ db::Track::find(session, listen.trackId) };
            if (!track)
                return false;

            dbListen = session.create<db::Listen>(user, track, db::ScrobblingBackend::ListenBrainz, listen.listenedAt);
            dbListen.modify()->setSyncState(scrobblingState);

            LMS_LOG_LISTENBRAINZ(DEBUG, "LISTEN CREATED for user " << user->getLoginName() << ", track '" << track->getName() << "' AT " << listen.listenedAt.toString());

            return true;
        }

        if (dbListen->getSyncState() == scrobblingState)
            return false;

        dbListen.modify()->setSyncState(scrobblingState);
        return true;
    }

    void ListensSynchronizer::enquePendingListens()
    {
        std::vector<TimedListen> pendingListens;

        {
            db::Session& session{ _db.getTLSSession() };

            auto transaction{ session.createWriteTransaction() };

            db::Listen::FindParameters params;
            params.setScrobblingBackend(db::ScrobblingBackend::ListenBrainz)
                .setSyncState(db::SyncState::PendingAdd)
                .setRange(db::Range{ 0, 100 }); // don't flood too much?

            const auto results{ db::Listen::find(session, params) };
            pendingListens.reserve(results.size());

            for (db::ListenId listenId : results)
            {
                const db::Listen::pointer listen{ db::Listen::find(session, listenId) };

                TimedListen timedListen;
                timedListen.listenedAt = listen->getDateTime();
                timedListen.userId = listen->getUser()->getId();
                timedListen.trackId = listen->getTrack()->getId();

                pendingListens.push_back(std::move(timedListen));
            }
        }

        LMS_LOG_LISTENBRAINZ(DEBUG, "Queing " << pendingListens.size() << " pending listen");

        for (const TimedListen& pendingListen : pendingListens)
            enqueListen(pendingListen);
    }

    ListensSynchronizer::UserContext& ListensSynchronizer::getUserContext(db::UserId userId)
    {
        assert(_strand.running_in_this_thread());

        auto itContext{ _userContexts.find(userId) };
        if (itContext == std::cend(_userContexts))
        {
            std::tie(itContext, std::ignore) = _userContexts.emplace(userId, userId);
        }

        return itContext->second;
    }

    bool ListensSynchronizer::isSyncing() const
    {
        return std::any_of(std::cbegin(_userContexts), std::cend(_userContexts), [](const auto& contextEntry) {
            return contextEntry.second.syncing;
        });
    }

    void ListensSynchronizer::scheduleSync(std::chrono::seconds fromNow)
    {
        if (_syncListensPeriod.count() == 0 || _maxSyncListenCount == 0)
            return;

        LMS_LOG_LISTENBRAINZ(DEBUG, "Scheduled sync in " << fromNow.count() << " seconds...");
        _syncTimer.expires_after(fromNow);
        _syncTimer.async_wait(boost::asio::bind_executor(_strand, [this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
            {
                LMS_LOG_LISTENBRAINZ(DEBUG, "getListens aborted");
                return;
            }
            else if (ec)
            {
                throw Exception{ "GetListens timer failure: " + std::string{ ec.message() } };
            }

            startSync();
        }));
    }

    void ListensSynchronizer::startSync()
    {
        LMS_LOG_LISTENBRAINZ(DEBUG, "Starting sync!");

        assert(!isSyncing());

        enquePendingListens();

        std::vector<db::UserId> userIds;
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };
            userIds = db::User::find(_db.getTLSSession(), db::User::FindParameters{}.setScrobblingBackend(db::ScrobblingBackend::ListenBrainz));
        }

        for (const db::UserId userId : userIds)
            startSync(getUserContext(userId));

        if (!isSyncing())
            scheduleSync(_syncListensPeriod);
    }

    void ListensSynchronizer::startSync(UserContext& context)
    {
        context.syncing = true;
        context.listenBrainzUserName = "";
        context.maxDateTime = {};
        context.fetchedListenCount = 0;
        context.matchedListenCount = 0;
        context.importedListenCount = 0;

        enqueValidateToken(context);
    }

    void ListensSynchronizer::onSyncEnded(UserContext& context)
    {
        boost::asio::post(boost::asio::bind_executor(_strand, [this, &context] {
            LMS_LOG_LISTENBRAINZ(INFO, "Sync done for user '" << context.listenBrainzUserName << "', fetched: " << context.fetchedListenCount << ", matched: " << context.matchedListenCount << ", imported: " << context.importedListenCount);
            context.syncing = false;

            if (!isSyncing())
                scheduleSync(_syncListensPeriod);
        }));
    }

    void ListensSynchronizer::enqueValidateToken(UserContext& context)
    {
        assert(context.listenBrainzUserName.empty());

        const std::string listenBrainzToken{ utils::getListenBrainzToken(_db.getTLSSession(), context.userId) };
        if (listenBrainzToken.empty())
        {
            onSyncEnded(context);
            return;
        }

        core::http::ClientGETRequestParameters request;
        request.priority = core::http::ClientRequestParameters::Priority::Low;
        request.relativeUrl = "/1/validate-token";
        request.headers = { { "Authorization", "Token " + listenBrainzToken } };
        request.onSuccessFunc = [this, &context](const Wt::Http::Message& msg) {
            context.listenBrainzUserName = utils::parseValidateToken(msg.body());
            if (context.listenBrainzUserName.empty())
            {
                onSyncEnded(context);
                return;
            }
            enqueGetListenCount(context);
        };
        request.onFailureFunc = [this, &context] {
            onSyncEnded(context);
        };

        _client.sendGETRequest(std::move(request));
    }

    void ListensSynchronizer::enqueGetListenCount(UserContext& context)
    {
        assert(!context.listenBrainzUserName.empty());

        core::http::ClientGETRequestParameters request;
        request.relativeUrl = "/1/user/" + std::string{ context.listenBrainzUserName } + "/listen-count";
        request.priority = core::http::ClientRequestParameters::Priority::Low;
        request.onSuccessFunc = [this, &context](const Wt::Http::Message& msg) {
            const auto listenCount{ parseListenCount(msg.body()) };
            boost::asio::post(boost::asio::bind_executor(_strand, [this, listenCount, &context] {
                if (listenCount)
                    LMS_LOG_LISTENBRAINZ(DEBUG, "Listen count for listenbrainz user '" << context.listenBrainzUserName << "' = " << *listenCount);

                bool needSync{ listenCount && (!context.listenCount || *context.listenCount != *listenCount) };
                context.listenCount = listenCount;

                if (!needSync)
                {
                    onSyncEnded(context);
                    return;
                }

                context.maxDateTime = Wt::WDateTime::currentDateTime();
                enqueGetListens(context);
            }));
        };
        request.onFailureFunc = [this, &context] {
            onSyncEnded(context);
        };

        _client.sendGETRequest(std::move(request));
    }

    void ListensSynchronizer::enqueGetListens(UserContext& context)
    {
        assert(!context.listenBrainzUserName.empty());

        core::http::ClientGETRequestParameters request;
        request.relativeUrl = "/1/user/" + context.listenBrainzUserName + "/listens?max_ts=" + std::to_string(context.maxDateTime.toTime_t());
        request.priority = core::http::ClientRequestParameters::Priority::Low;
        request.onSuccessFunc = [this, &context](const Wt::Http::Message& msg) {
            processGetListensResponse(msg.body(), context);
            if (context.fetchedListenCount >= _maxSyncListenCount || !context.maxDateTime.isValid())
            {
                onSyncEnded(context);
                return;
            }

            enqueGetListens(context);
        };
        request.onFailureFunc = [this, &context] {
            onSyncEnded(context);
        };

        _client.sendGETRequest(std::move(request));
    }

    void ListensSynchronizer::processGetListensResponse(std::string_view msgBody, UserContext& context)
    {
        db::Session& session{ _db.getTLSSession() };

        context.maxDateTime = {}; // invalidate to break in case no more listens are fetched
        ListensParser::Result result{ ListensParser::parse(msgBody) };
        context.fetchedListenCount += result.listenCount;

        for (const Listen& parsedListen : result.listens)
        {
            // update oldest listen for the next query
            if (!parsedListen.listenedAt.isValid())
            {
                LMS_LOG_LISTENBRAINZ(DEBUG, "Skipping entry due to invalid listenedAt");
                continue;
            }

            if (!context.maxDateTime.isValid() || context.maxDateTime > parsedListen.listenedAt)
                context.maxDateTime = parsedListen.listenedAt;

            if (const db::TrackId trackId{ tryGetMatchingTrack(session, parsedListen) }; trackId.isValid())
            {
                context.matchedListenCount++;

                const scrobbling::TimedListen listen{ { context.userId, trackId }, parsedListen.listenedAt };
                if (saveListen(listen, db::SyncState::Synchronized))
                    context.importedListenCount++;
            }
        }
    }
} // namespace lms::scrobbling::listenBrainz
