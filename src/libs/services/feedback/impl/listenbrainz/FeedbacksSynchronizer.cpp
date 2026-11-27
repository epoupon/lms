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

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <unordered_map>

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
#include "database/ISession.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackFeedback.hpp"
#include "database/objects/TrackFeedbackBackendSync.hpp"
#include "database/objects/User.hpp"

#include "FeedbacksParser.hpp"
#include "Utils.hpp"

namespace lms::feedback::listenBrainz
{
    namespace
    {
        // 1, 2, 4 ... 64 minutes
        constexpr std::size_t maxExportRetryDoublings{ 6 };
        std::chrono::minutes getExportRetryDelay(std::size_t consecutiveFailures)
        {
            return std::chrono::minutes{ 1 << std::min(consecutiveFailures, maxExportRetryDoublings) };
        }

        Wt::Json::Object feedbackToJsonPayload(const core::UUID& recordingMBID, db::FeedbackValue value)
        {
            Wt::Json::Object root;
            root["recording_mbid"] = Wt::Json::Value{ recordingMBID.toString() };
            root["score"] = Wt::Json::Value{ utils::toLBScore(value) };
            return root;
        }

        // Drops the sync row for a feedback that can never be delivered (no recording MBID),
        // so it stops being retried on every flush. Assumes an already-open transaction on `session`.
        void dropFeedbackBackendSync(db::ISession& session, db::TrackFeedbackId id)
        {
            if (const std::optional<db::TrackFeedbackBackendSync> sync{ db::TrackFeedbackBackendSync::find(session, id, db::FeedbackBackend::ListenBrainz) })
                db::TrackFeedbackBackendSync::destroy(session, std::array{ sync->getId() });
        }

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
        , _maxSyncFeedbackCount{ core::Service<core::IConfig>::get()->getULong("listenbrainz-max-sync-feedback-count", 0) }
    {
        LOG(INFO, "Starting Feedbacks synchronizer, maxSyncFeedbackCount = " << _maxSyncFeedbackCount);

        // resume the exports interrupted by the last shutdown
        boost::asio::post(_ioContext, [this] { requestExportForEnabledUsers(); });
    }

    void FeedbacksSynchronizer::enqueFeedback(db::TrackFeedbackId id)
    {
        db::UserId userId;
        {
            db::ISession& session{ _db.getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            const std::optional<db::TrackFeedback> trackFeedback{ db::TrackFeedback::find(session, id) };
            if (!trackFeedback)
                return;

            userId = trackFeedback->getUserId();

            std::optional<db::TrackFeedbackBackendSync> sync{ db::TrackFeedbackBackendSync::find(session, id, db::FeedbackBackend::ListenBrainz) };
            if (!sync)
            {
                db::TrackFeedbackBackendSync::create(session, trackFeedback->getId(), db::FeedbackBackend::ListenBrainz);
            }
            else if (sync->getSyncState() != db::SyncState::PendingAdd)
            {
                sync->setSyncState(db::SyncState::PendingAdd);
                db::TrackFeedbackBackendSync::update(session, *sync);
            }
        }

        boost::asio::post(boost::asio::bind_executor(_strand, [this, userId] {
            startExport(getUserContext(userId));
        }));
    }

    void FeedbacksSynchronizer::dropFeedback(db::TrackFeedbackId id)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };
        dropFeedbackBackendSync(session, id);
    }

    void FeedbacksSynchronizer::onFeedbackSent(db::TrackFeedbackId id, db::FeedbackValue sentValue)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        const std::optional<db::TrackFeedback> trackFeedback{ db::TrackFeedback::find(session, id) };
        if (!trackFeedback)
        {
            LOG(DEBUG, "Track feedback not found. deleted?");
            return;
        }

        // changed while being sent: the new value stays queued
        if (trackFeedback->getValue() != sentValue)
            return;

        if (std::optional<db::TrackFeedbackBackendSync> sync{ db::TrackFeedbackBackendSync::find(session, id, db::FeedbackBackend::ListenBrainz) })
        {
            sync->setSyncState(db::SyncState::Synchronized);
            db::TrackFeedbackBackendSync::update(session, *sync);
        }
    }

    void FeedbacksSynchronizer::requestExportForEnabledUsers()
    {
        std::vector<db::UserId> userIds;
        {
            db::ISession& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::User::find(session, db::User::FindParameters{}, [&](const db::User& user) {
                if (user.getFeedbackBackends().contains(db::FeedbackBackend::ListenBrainz))
                    userIds.push_back(user.getId());
            });
        }

        for (const db::UserId userId : userIds)
            requestImmediateExport(userId);
    }

    void FeedbacksSynchronizer::sendNextFeedback(UserContext& context)
    {
        assert(_strand.running_in_this_thread());

        const std::string listenBrainzToken{ utils::getListenBrainzToken(_db.getTLSSession(), context.userId) };
        if (listenBrainzToken.empty())
        {
            context.exportState.exporting = false;
            return;
        }

        db::TrackFeedbackId id;
        core::UUID recordingMBID;
        db::FeedbackValue value{};
        while (!id.isValid())
        {
            db::TrackFeedbackId pendingId;
            std::optional<core::UUID> pendingRecordingMBID;
            {
                db::ISession& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::TrackFeedbackBackendSync::FindParameters params;
                params.setUser(context.userId)
                    .setBackend(db::FeedbackBackend::ListenBrainz)
                    .setSyncState(db::SyncState::PendingAdd)
                    .setRange(db::Range{ 0, 1 });

                db::TrackFeedbackBackendSync::find(session, params, [&](const db::TrackFeedbackBackendSync& sync) {
                    const std::optional<db::TrackFeedback> trackFeedback{ db::TrackFeedback::find(session, sync.getTrackFeedbackId()) };
                    pendingId = trackFeedback->getId();
                    value = trackFeedback->getValue();
                    if (utils::canBeFeedbacked(session, trackFeedback->getTrackId()))
                        pendingRecordingMBID = db::Track::find(session, trackFeedback->getTrackId())->getRecordingMBID();
                });
            }

            if (!pendingId.isValid())
            {
                context.exportState.exporting = false;
                return;
            }

            if (!pendingRecordingMBID)
            {
                LOG(DEBUG, "Feedback cannot be sent (no recording MBID): dropping it");
                dropFeedback(pendingId);
                continue;
            }

            id = pendingId;
            recordingMBID = *pendingRecordingMBID;
        }

        core::http::ClientPOSTRequestParameters request;
        request.relativeUrl = "/1/feedback/recording-feedback";
        request.message.addHeader("Authorization", "Token " + listenBrainzToken);
        request.message.addBodyText(Wt::Json::serialize(feedbackToJsonPayload(recordingMBID, value)));
        request.message.addHeader("Content-Type", "application/json");
        request.onSuccessFunc = [this, id, value, &context](const Wt::Http::Message&) {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, id, value, &context] {
                onFeedbackSent(id, value);

                context.exportState.consecutiveFailures = 0;
                sendNextFeedback(context);
            }));
        };
        request.onFailureFunc = [this, id, &context](std::optional<int> httpStatus) {
            boost::asio::post(boost::asio::bind_executor(_strand, [this, id, httpStatus, &context] {
                // invalid feedback (unknown recording MBID...): sending it again cannot work
                if (httpStatus == 400)
                {
                    LOG(ERROR, "Server rejected a feedback (HTTP status 400): dropping it");
                    dropFeedback(id);
                    sendNextFeedback(context);
                    return;
                }

                scheduleExportRetry(context);
            }));
        };

        _client.sendPOSTRequest(std::move(request));
    }

    void FeedbacksSynchronizer::scheduleExportRetry(UserContext& context)
    {
        assert(_strand.running_in_this_thread());

        const std::size_t failureCount{ context.exportState.consecutiveFailures++ };
        const std::chrono::minutes retryDelay{ getExportRetryDelay(failureCount) };
        if (failureCount == maxExportRetryDoublings)
            LOG(WARNING, "Still cannot send feedbacks for user " << context.userId.toString() << ", now retrying every " << retryDelay.count() << " minutes: is the server down or misconfigured?");
        else
            LOG(INFO, "Cannot send feedbacks, retrying in " << retryDelay.count() << " minutes");

        context.exportState.retryTimer.expires_after(retryDelay);
        context.exportState.retryTimer.async_wait(boost::asio::bind_executor(_strand, [this, &context](const boost::system::error_code& ec) {
            if (ec)
                return; // cancelled on shutdown or by requestImmediateExport

            sendNextFeedback(context);
        }));
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
            UserContext& context{ getUserContext(userId) };
            // asked for by the user (backend turned on, token changed): no reason to wait for the end of a retry delay
            if (context.exportState.retryTimer.cancel() > 0)
            {
                sendNextFeedback(context);
                return;
            }

            startExport(context);
        }));
    }

    void FeedbacksSynchronizer::startExport(UserContext& context)
    {
        assert(_strand.running_in_this_thread());

        // the running export picks up what was queued meanwhile, and a pending retry keeps its delay
        if (context.exportState.exporting)
            return;

        context.exportState.exporting = true;
        sendNextFeedback(context);
    }

    FeedbacksSynchronizer::UserContext& FeedbacksSynchronizer::getUserContext(db::UserId userId)
    {
        assert(_strand.running_in_this_thread());

        auto itContext{ _userContexts.find(userId) };
        if (itContext == std::cend(_userContexts))
        {
            std::tie(itContext, std::ignore) = _userContexts.try_emplace(userId, userId, _ioContext);
        }

        return itContext->second;
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
        request.onFailureFunc = [this, &context](std::optional<int>) {
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
        request.onFailureFunc = [this, &context](std::optional<int>) {
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
                const bool limitReached{ _maxSyncFeedbackCount != 0 && context.import.fetchedFeedbackCount >= _maxSyncFeedbackCount };
                if (fetchedFeedbackCount == 0                                       // no more thing available on server
                    || context.import.fetchedFeedbackCount >= context.feedbackCount // we may miss something, but we will get it next time
                    || limitReached)
                {
                    if (limitReached && context.feedbackCount && *context.feedbackCount > context.import.fetchedFeedbackCount)
                        LOG(INFO, "Import reached its limit of " << _maxSyncFeedbackCount << " feedbacks: " << (*context.feedbackCount - context.import.fetchedFeedbackCount) << " older feedbacks not fetched");

                    onImportEnded(context);
                }
                else
                {
                    enqueGetFeedbacks(context);
                }
            }));
        };
        request.onFailureFunc = [this, &context](std::optional<int>) {
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

        ISession& session{ _db.getTLSSession() };

        bool needImport{};
        TrackId trackId;

        {
            auto transaction{ session.createReadTransaction() };
            const std::vector<Track> tracks{ Track::findByRecordingMBID(session, feedback.recordingMBID) };
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

            trackId = tracks.front().getId();

            const std::optional<TrackFeedback> trackFeedback{ TrackFeedback::find(session, trackId, context.userId) };
            const std::optional<TrackFeedbackBackendSync> sync{ trackFeedback ? TrackFeedbackBackendSync::find(session, trackFeedback->getId(), FeedbackBackend::ListenBrainz) : std::optional<TrackFeedbackBackendSync>{} };

            // If nothing is tracked locally yet for this backend, import it. If something is already queued
            // to be sent to the backend, let that win (it'll be resent later). Only re-import when we're fully
            // synchronized but the remote value has since diverged (changed via another ListenBrainz client).
            needImport = !sync || (sync->getSyncState() == SyncState::Synchronized && trackFeedback->getValue() != feedback.score);
        }

        if (needImport)
        {
            LOG(DEBUG, "Importing feedback '" << feedback << "'");

            auto transaction{ session.createWriteTransaction() };

            const std::optional<Track> track{ Track::find(session, trackId) };
            if (!track)
                return;

            const std::optional<User> user{ User::find(session, context.userId) };
            if (!user)
                return;

            std::optional<TrackFeedback> trackFeedback{ TrackFeedback::find(session, trackId, context.userId) };
            if (!trackFeedback)
                trackFeedback = TrackFeedback::create(session, track->getId(), user->getId());

            trackFeedback->setValue(feedback.score);
            trackFeedback->setDateTime(feedback.created);
            TrackFeedback::update(session, *trackFeedback);

            std::optional<TrackFeedbackBackendSync> sync{ TrackFeedbackBackendSync::find(session, trackFeedback->getId(), FeedbackBackend::ListenBrainz) };
            if (!sync)
                sync = TrackFeedbackBackendSync::create(session, trackFeedback->getId(), FeedbackBackend::ListenBrainz);
            sync->setSyncState(SyncState::Synchronized);
            TrackFeedbackBackendSync::update(session, *sync);

            context.import.importedFeedbackCount++;
        }
        else
        {
            LOG(DEBUG, "No need to import feedback '" << feedback << "', already imported");
            context.import.matchedFeedbackCount++;
        }
    }
} // namespace lms::feedback::listenBrainz
