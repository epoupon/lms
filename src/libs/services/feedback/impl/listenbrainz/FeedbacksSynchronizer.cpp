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

#include "FeedbacksSynchronizer.hpp"

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
#include "database/objects/Track.hpp"
#include "database/objects/TrackFeedback.hpp"
#include "database/objects/TrackFeedbackBackendSync.hpp"
#include "database/objects/User.hpp"

#include "Exception.hpp"
#include "FeedbacksParser.hpp"
#include "Utils.hpp"

namespace lms::feedback::listenBrainz
{
    namespace
    {
        std::optional<std::size_t> parseTotalFeedbackCount(std::string_view msgBody)
        {
            try
            {
                Wt::Json::Object root;
                Wt::Json::parse(std::string{ msgBody }, root);

                return static_cast<int>(root.get("total_count"));
            }
            catch (const Wt::WException& e)
            {
                LOG(ERROR, "Cannot parse listen count response: " << e.what());
                return std::nullopt;
            }
        }
    } // namespace

    FeedbacksSynchronizer::FeedbacksSynchronizer(boost::asio::io_context& ioContext, db::IDb& db, core::http::IClient& client)
        : _ioContext{ ioContext }
        , _db{ db }
        , _client{ client }
        , _maxSyncFeedbackCount{ core::Service<core::IConfig>::get()->getULong("listenbrainz-max-sync-feedback-count", 1000) }
        , _syncFeedbacksPeriod{ core::Service<core::IConfig>::get()->getULong("listenbrainz-sync-feedbacks-period-hours", 1) }
    {
        LOG(INFO, "Starting Feedbacks synchronizer, maxSyncFeedbackCount = " << _maxSyncFeedbackCount << ", _syncFeedbacksPeriod = " << _syncFeedbacksPeriod.count() << " hours");

        scheduleDeliveryFlush(std::chrono::seconds{ 30 });
    }

    void FeedbacksSynchronizer::enqueFeedback(db::TrackFeedbackId id)
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        db::TrackFeedback::pointer trackFeedback{ db::TrackFeedback::find(session, id) };
        if (!trackFeedback)
            return;

        const std::optional<core::UUID> recordingMBID{ trackFeedback->getTrack()->getRecordingMBID() };
        if (!recordingMBID)
        {
            LOG(DEBUG, "Track has no recording MBID: skipping");
            return;
        }

        const std::string listenBrainzToken{ trackFeedback->getUser()->getListenBrainzToken() };
        if (listenBrainzToken.empty())
            return;

        db::TrackFeedbackBackendSync::pointer sync{ db::TrackFeedbackBackendSync::find(session, id, db::FeedbackBackend::ListenBrainz) };
        if (!sync)
            sync = session.create<db::TrackFeedbackBackendSync>(trackFeedback, db::FeedbackBackend::ListenBrainz);
        else if (sync->getSyncState() != db::SyncState::PendingAdd)
            sync.modify()->setSyncState(db::SyncState::PendingAdd);

        core::http::ClientPOSTRequestParameters request;
        request.relativeUrl = "/1/feedback/recording-feedback";
        request.message.addHeader("Authorization", "Token " + listenBrainzToken);

        Wt::Json::Object root;
        root["recording_mbid"] = Wt::Json::Value{ recordingMBID->toString() };
        root["score"] = Wt::Json::Value{ utils::toLBScore(trackFeedback->getValue()) };

        request.message.addBodyText(Wt::Json::serialize(root));
        request.message.addHeader("Content-Type", "application/json");

        request.onSuccessFunc = [this, id](const Wt::Http::Message&) {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, id] {
                onFeedbackSent(id);
            }));
        };
        _client.sendPOSTRequest(std::move(request));
    }

    void FeedbacksSynchronizer::onFeedbackSent(db::TrackFeedbackId id)
    {
        assert(_strand.running_in_this_thread());

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        db::TrackFeedback::pointer trackFeedback{ db::TrackFeedback::find(session, id) };
        if (!trackFeedback)
        {
            LOG(DEBUG, "Track feedback not found. deleted?");
            return;
        }

        if (db::TrackFeedbackBackendSync::pointer sync{ db::TrackFeedbackBackendSync::find(session, id, db::FeedbackBackend::ListenBrainz) })
            sync.modify()->setSyncState(db::SyncState::Synchronized);

        LOG(DEBUG, "State set to synchronized");
    }

    void FeedbacksSynchronizer::enquePendingFeedbacks()
    {
        using namespace db;

        std::vector<TrackFeedbackId> pendingFeedbacks;
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            TrackFeedbackBackendSync::FindParameters params;
            params.setBackend(db::FeedbackBackend::ListenBrainz)
                .setSyncState(SyncState::PendingAdd)
                .setRange(db::Range{ 0, 100 }); // don't flood too much?

            TrackFeedbackBackendSync::find(session, params, [&](const TrackFeedbackBackendSync::pointer& sync) {
                pendingFeedbacks.push_back(sync->getTrackFeedback()->getId());
            });
        }

        LOG(DEBUG, "Queing " << pendingFeedbacks.size() << " pending feedbacks");

        for (const TrackFeedbackId id : pendingFeedbacks)
            enqueFeedback(id);
    }

    void FeedbacksSynchronizer::markPendingExports(db::UserId userId)
    {
        using namespace db;

        constexpr std::size_t chunkSize{ 500 };
        TrackFeedbackId lastRetrievedId;
        for (;;)
        {
            std::vector<TrackFeedbackId> ids;
            std::size_t rawCount{};
            {
                Session& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                TrackFeedback::FindParameters params;
                params.setUser(userId);
                params.setLastRetrievedId(lastRetrievedId);
                params.setRange(Range{ 0, chunkSize });
                params.setSortMethod(TrackFeedbackSortMethod::Id);
                TrackFeedback::find(session, params, [&](const TrackFeedback::pointer& feedback) {
                    ++rawCount;
                    lastRetrievedId = feedback->getId();
                    if (feedback->getValue() != FeedbackValue::None)
                        ids.push_back(feedback->getId());
                });
            }

            if (rawCount == 0)
                break;

            {
                Session& session{ _db.getTLSSession() };
                auto transaction{ session.createWriteTransaction() };

                for (const TrackFeedbackId id : ids)
                {
                    if (TrackFeedbackBackendSync::find(session, id, FeedbackBackend::ListenBrainz))
                        continue;

                    if (TrackFeedback::pointer trackFeedback{ TrackFeedback::find(session, id) })
                        session.create<TrackFeedbackBackendSync>(trackFeedback, FeedbackBackend::ListenBrainz);
                }
            }

            if (rawCount < chunkSize)
                break;
        }
    }

    void FeedbacksSynchronizer::requestImmediateImport(db::UserId userId)
    {
        boost::asio::post(boost::asio::bind_executor(_strand, [this, userId] {
            UserContext& context{ getUserContext(userId) };
            if (context.import.importing)
            {
                LOG(DEBUG, "Import already in progress for this user, ignoring manual trigger");
                return;
            }

            startImport(context);
        }));
    }

    void FeedbacksSynchronizer::requestImmediateExport(db::UserId userId)
    {
        boost::asio::post(boost::asio::bind_executor(_strand, [this, userId] {
            markPendingExports(userId);
            scheduleDeliveryFlush(std::chrono::seconds{ 0 });
        }));
    }

    FeedbacksSynchronizer::UserContext& FeedbacksSynchronizer::getUserContext(db::UserId userId)
    {
        assert(_strand.running_in_this_thread());

        auto itContext{ _userContexts.find(userId) };
        if (itContext == std::cend(_userContexts))
        {
            std::tie(itContext, std::ignore) = _userContexts.emplace(userId, userId);
        }

        return itContext->second;
    }

    void FeedbacksSynchronizer::scheduleDeliveryFlush(std::chrono::seconds fromNow)
    {
        if (_syncFeedbacksPeriod.count() == 0 || _maxSyncFeedbackCount == 0)
            return;

        LOG(DEBUG, "Scheduled delivery flush in " << fromNow.count() << " seconds...");
        _syncTimer.expires_after(fromNow);
        _syncTimer.async_wait(boost::asio::bind_executor(_strand, [this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
            {
                LOG(DEBUG, "Periodic delivery flush timer aborted");
                return;
            }

            if (ec)
                throw Exception{ "Delivery flush timer failure: " + std::string{ ec.message() } };

            flushPendingDeliveries();
        }));
    }

    void FeedbacksSynchronizer::flushPendingDeliveries()
    {
        LOG(DEBUG, "Flushing pending deliveries...");

        enquePendingFeedbacks();

        scheduleDeliveryFlush(_syncFeedbacksPeriod);
    }

    void FeedbacksSynchronizer::startImport(UserContext& context)
    {
        context.import.importing = true;
        context.import.listenBrainzUserName = "";
        context.import.fetchedFeedbackCount = 0;
        context.import.matchedFeedbackCount = 0;
        context.import.importedFeedbackCount = 0;

        enqueValidateToken(context);
    }

    void FeedbacksSynchronizer::onImportEnded(UserContext& context)
    {
        boost::asio::post(boost::asio::bind_executor(_strand, [this, &context] {
            LOG(INFO, "Feedback import done for user '" << context.import.listenBrainzUserName << "', fetched: " << context.import.fetchedFeedbackCount << ", matched: " << context.import.matchedFeedbackCount << ", imported: " << context.import.importedFeedbackCount);
            context.import.importing = false;
        }));
    }

    void FeedbacksSynchronizer::enqueValidateToken(UserContext& context)
    {
        assert(context.import.listenBrainzUserName.empty());

        const std::string listenBrainzToken{ utils::getListenBrainzToken(_db.getTLSSession(), context.userId) };
        if (listenBrainzToken.empty())
        {
            onImportEnded(context);
            return;
        }

        core::http::ClientGETRequestParameters request;
        request.priority = core::http::ClientRequestParameters::Priority::Low;
        request.relativeUrl = "/1/validate-token";
        request.headers = { { "Authorization", "Token " + listenBrainzToken } };
        request.onSuccessFunc = [this, &context](const Wt::Http::Message& msg) {
            std::string listenBrainzUserName{ utils::parseValidateToken(msg.body()) };
            boost::asio::post(boost::asio::bind_executor(_strand, [this, listenBrainzUserName = std::move(listenBrainzUserName), &context]() mutable {
                context.import.listenBrainzUserName = std::move(listenBrainzUserName);
                if (context.import.listenBrainzUserName.empty())
                {
                    onImportEnded(context);
                    return;
                }
                enqueGetFeedbackCount(context);
            }));
        };
        request.onFailureFunc = [this, &context] {
            onImportEnded(context);
        };

        _client.sendGETRequest(std::move(request));
    }

    void FeedbacksSynchronizer::enqueGetFeedbackCount(UserContext& context)
    {
        assert(!context.import.listenBrainzUserName.empty());

        core::http::ClientGETRequestParameters request;
        request.relativeUrl = "/1/feedback/user/" + std::string{ context.import.listenBrainzUserName } + "/get-feedback?count=0";
        request.priority = core::http::ClientRequestParameters::Priority::Low;
        request.onSuccessFunc = [this, &context](const Wt::Http::Message& msg) {
            std::string msgBodyCopy{ msg.body() };
            boost::asio::post(boost::asio::bind_executor(_strand, [this, msgBodyCopy, &context] {
                LOG(DEBUG, "Current feedback count = " << (context.feedbackCount ? *context.feedbackCount : 0) << " for user '" << context.import.listenBrainzUserName << "'");

                const auto totalFeedbackCount = parseTotalFeedbackCount(msgBodyCopy);
                if (totalFeedbackCount)
                    LOG(DEBUG, "Feedback count for listenbrainz user '" << context.import.listenBrainzUserName << "' = " << *totalFeedbackCount);

                bool needImport{ totalFeedbackCount && (!context.feedbackCount || *context.feedbackCount != *totalFeedbackCount) };
                context.feedbackCount = totalFeedbackCount;

                if (needImport)
                    enqueGetFeedbacks(context);
                else
                    onImportEnded(context);
            }));
        };
        request.onFailureFunc = [this, &context] {
            onImportEnded(context);
        };

        _client.sendGETRequest(std::move(request));
    }

    void FeedbacksSynchronizer::enqueGetFeedbacks(UserContext& context)
    {
        assert(!context.import.listenBrainzUserName.empty());

        core::http::ClientGETRequestParameters request;
        request.relativeUrl = "/1/feedback/user/" + context.import.listenBrainzUserName + "/get-feedback?offset=" + std::to_string(context.import.fetchedFeedbackCount);
        request.priority = core::http::ClientRequestParameters::Priority::Low;
        request.onSuccessFunc = [this, &context](const Wt::Http::Message& msg) {
            std::string msgBodyCopy{ msg.body() };
            boost::asio::post(boost::asio::bind_executor(_strand, [this, msgBodyCopy, &context] {
                const std::size_t fetchedFeedbackCount{ processGetFeedbacks(msgBodyCopy, context) };
                if (fetchedFeedbackCount == 0                                       // no more thing available on server
                    || context.import.fetchedFeedbackCount >= context.feedbackCount // we may miss something, but we will get it next time
                    || context.import.fetchedFeedbackCount >= _maxSyncFeedbackCount)
                {
                    onImportEnded(context);
                }
                else
                {
                    enqueGetFeedbacks(context);
                }
            }));
        };
        request.onFailureFunc = [this, &context] {
            onImportEnded(context);
        };

        _client.sendGETRequest(std::move(request));
    }

    std::size_t FeedbacksSynchronizer::processGetFeedbacks(std::string_view msgBody, UserContext& context)
    {
        const FeedbacksParser::Result parseResult{ FeedbacksParser::parse(msgBody) };

        LOG(DEBUG, "Parsed " << parseResult.feedbackCount << " feedbacks, found " << parseResult.feedbacks.size() << " usable entries");
        context.import.fetchedFeedbackCount += parseResult.feedbackCount;

        for (const Feedback& feedback : parseResult.feedbacks)
        {
            tryImportFeedback(feedback, context);
        }

        return parseResult.feedbackCount;
    }

    void FeedbacksSynchronizer::tryImportFeedback(const Feedback& feedback, UserContext& context)
    {
        using namespace db;

        Session& session{ _db.getTLSSession() };

        bool needImport{};
        TrackId trackId;

        {
            auto transaction{ session.createReadTransaction() };
            const std::vector<Track::pointer> tracks{ Track::findByRecordingMBID(session, feedback.recordingMBID) };
            if (tracks.size() > 1)
            {
                LOG(DEBUG, "Too many matches for feedback '" << feedback << "': duplicate recording MBIDs found");
                return;
            }

            if (tracks.empty())
            {
                LOG(DEBUG, "Cannot match feedback '" << feedback << "': no track found for this recording MBID");
                return;
            }

            trackId = tracks.front()->getId();

            const TrackFeedback::pointer trackFeedback{ TrackFeedback::find(session, trackId, context.userId) };
            const TrackFeedbackBackendSync::pointer sync{ trackFeedback ? TrackFeedbackBackendSync::find(session, trackFeedback->getId(), FeedbackBackend::ListenBrainz) : TrackFeedbackBackendSync::pointer{} };

            // If nothing is tracked locally yet for this backend, import it. If something is already queued
            // to be sent to the backend, let that win (it'll be resent later). Only re-import when we're fully
            // synchronized but the remote value has since diverged (changed via another ListenBrainz client).
            needImport = !sync || (sync->getSyncState() == SyncState::Synchronized && trackFeedback->getValue() != feedback.score);
        }

        if (needImport)
        {
            LOG(DEBUG, "Importing feedback '" << feedback << "'");

            auto transaction{ session.createWriteTransaction() };

            const Track::pointer track{ Track::find(session, trackId) };
            if (!track)
                return;

            const User::pointer user{ User::find(session, context.userId) };
            if (!user)
                return;

            TrackFeedback::pointer trackFeedback{ TrackFeedback::find(session, trackId, context.userId) };
            if (!trackFeedback)
                trackFeedback = session.create<TrackFeedback>(track, user);

            trackFeedback.modify()->setValue(feedback.score);
            trackFeedback.modify()->setDateTime(feedback.created);

            TrackFeedbackBackendSync::pointer sync{ TrackFeedbackBackendSync::find(session, trackFeedback->getId(), FeedbackBackend::ListenBrainz) };
            if (!sync)
                sync = session.create<TrackFeedbackBackendSync>(trackFeedback, FeedbackBackend::ListenBrainz);
            sync.modify()->setSyncState(SyncState::Synchronized);

            context.import.importedFeedbackCount++;
        }
        else
        {
            LOG(DEBUG, "No need to import feedback '" << feedback << "', already imported");
            context.import.matchedFeedbackCount++;
        }
    }
} // namespace lms::feedback::listenBrainz
