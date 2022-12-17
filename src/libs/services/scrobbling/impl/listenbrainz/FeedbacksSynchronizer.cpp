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

#include "ListenBrainzScrobbler.hpp"

#include <tuple>
#include <boost/asio/bind_executor.hpp>
#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Serializer.h>

#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/database/StarredTrack.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/IConfig.hpp"
#include "utils/http/IClient.hpp"
#include "utils/Service.hpp"

#include "Exception.hpp"
#include "FeedbacksParser.hpp"
#include "Utils.hpp"

using namespace Scrobbling::ListenBrainz;
using namespace Database;

namespace
{
	std::optional<std::size_t>
	parseTotalFeedbackCount(std::string_view msgBody)
	{
		try
		{
			Wt::Json::Object root;
			Wt::Json::parse(std::string {msgBody}, root);

			return static_cast<int>(root.get("total_count"));
		}
		catch (const Wt::WException& e)
		{
			LOG(ERROR) << "Cannot parse listen count response: " << e.what();
			return std::nullopt;
		}
	}
}

namespace Scrobbling::ListenBrainz
{
	FeedbacksSynchronizer::FeedbacksSynchronizer(boost::asio::io_context& ioContext, Database::Db& db, Http::IClient& client)
		: _ioContext {ioContext}
		, _db {db}
		, _client {client}
		, _maxSyncFeedbackCount {Service<IConfig>::get()->getULong("listenbrainz-max-sync-feedback-count", 1000)}
		, _syncFeedbacksPeriod {Service<IConfig>::get()->getULong("listenbrainz-sync-feedbacks-period-hours", 1)}
	{
		LOG(INFO) << "Starting Feedbacks synchronizer, maxSyncFeedbackCount = " << _maxSyncFeedbackCount << ", _syncFeedbacksPeriod = " << _syncFeedbacksPeriod.count() << " hours";

		scheduleSync(std::chrono::seconds {30});
	}

	void
	FeedbacksSynchronizer::enqueFeedback(FeedbackType type, Database::StarredTrackId starredTrackId)
	{
		try
		{
			Database::Session& session {_db.getTLSSession()};

			auto transaction {session.createUniqueTransaction()};

			StarredTrack::pointer starredTrack {StarredTrack::find(session, starredTrackId)};
			if (!starredTrack)
				return;

			std::optional<UUID> recordingMBID {starredTrack->getTrack()->getRecordingMBID()};

			switch (type)
			{
				case FeedbackType::Love:
					if (starredTrack->getScrobblingState() != ScrobblingState::PendingAdd)
						starredTrack.modify()->setScrobblingState(ScrobblingState::PendingAdd);
					break;

				case FeedbackType::Erase:
					if (!recordingMBID)
					{
						LOG(DEBUG) << "Track has no recording MBID: erasing star";
						starredTrack.remove();
					}
					else
					{
						// Send the erase order even if it is not on the remote LB server (it may be
						// queued for add, or not)
						starredTrack.modify()->setScrobblingState(ScrobblingState::PendingRemove);
					}
					break;

				default:
					throw Exception {"Unhandled feedback type"};
			}

			if (!recordingMBID)
			{
				LOG(DEBUG) << "Track has no recording MBID: skipping";
				return;
			}

			const std::optional<UUID> listenBrainzToken {starredTrack->getUser()->getListenBrainzToken()};
			if (!listenBrainzToken)
				return;

			Http::ClientPOSTRequestParameters request;
			request.relativeUrl = "/1/feedback/recording-feedback";
			request.message.addHeader("Authorization", "Token " + std::string {listenBrainzToken->getAsString()});

			Wt::Json::Object root;
			root["recording_mbid"] = Wt::Json::Value {std::string {recordingMBID->getAsString()}};
			root["score"] = Wt::Json::Value {static_cast<int>(type)};

			request.message.addBodyText(Wt::Json::serialize(root));
			request.message.addHeader("Content-Type", "application/json");

			request.onSuccessFunc = [=](std::string_view /*msgBody*/)
			{
				_strand.dispatch([=]
				{
					onFeedbackSent(type, starredTrackId);
				});
			};
			_client.sendPOSTRequest(std::move(request));
		}
		catch (Exception& e)
		{
			LOG(DEBUG) << "Cannot send feedback: " << e.what();
		}
	}

	void
	FeedbacksSynchronizer::onFeedbackSent(FeedbackType type, Database::StarredTrackId starredTrackId)
	{
		assert(_strand.running_in_this_thread());

		Session& session {_db.getTLSSession()};
		auto transaction {session.createUniqueTransaction()};

		StarredTrack::pointer starredTrack {StarredTrack::find(session, starredTrackId)};
		if (!starredTrack)
		{
			LOG(DEBUG) << "Starred track not found. deleted?";
			return;
		}

		UserContext& userContext {getUserContext(starredTrack->getUser()->getId())};

		switch (type)
		{
			case FeedbackType::Love:
				starredTrack.modify()->setScrobblingState(ScrobblingState::Synchronized);
				LOG(DEBUG) << "State set to synchronized";

				if (userContext.feedbackCount)
				{
					(*userContext.feedbackCount)++;
					LOG(DEBUG) << "Feedback count set to " << *userContext.feedbackCount << " for user '" << userContext.listenBrainzUserName <<"'";
				}
				break;

			case FeedbackType::Erase:
				starredTrack.remove();
				LOG(DEBUG) << "Removed starred track";

				if (userContext.feedbackCount && *userContext.feedbackCount > 0)
				{
					(*userContext.feedbackCount)--;
					LOG(DEBUG) << "Feedback count set to " << *userContext.feedbackCount << " for user '" << userContext.listenBrainzUserName <<"'";
				}
				break;

			default:
				throw Exception {"Unhandled feedback type"};
		}
	}

	void
	FeedbacksSynchronizer::enquePendingFeedbacks()
	{
		auto processPendingFeedbacks { [this] (ScrobblingState scrobblingState, FeedbackType feedbackType)
		{
			RangeResults<StarredTrackId> pendingFeedbacks;

			{
				Database::Session& session {_db.getTLSSession()};

				auto transaction {session.createSharedTransaction()};

				StarredTrack::FindParameters params;
				params.setScrobbler(Database::Scrobbler::ListenBrainz, scrobblingState)
						.setRange(Database::Range {0, 100}); // don't flood too much?

				pendingFeedbacks = StarredTrack::find(session, params);
			}

			LOG(DEBUG) << "Queing " << pendingFeedbacks.results.size() << " pending '" << (feedbackType == FeedbackType::Love ? "love" : "erase") << "' feedbacks";

			for (const StarredTrackId starredTrackId : pendingFeedbacks.results)
				enqueFeedback(feedbackType, starredTrackId);
		}};

		processPendingFeedbacks(ScrobblingState::PendingAdd, FeedbackType::Love);
		processPendingFeedbacks(ScrobblingState::PendingRemove, FeedbackType::Erase);
	}

	FeedbacksSynchronizer::UserContext&
	FeedbacksSynchronizer::getUserContext(Database::UserId userId)
	{
		assert(_strand.running_in_this_thread());

		auto itContext {_userContexts.find(userId)};
		if (itContext == std::cend(_userContexts))
		{
			std::tie(itContext, std::ignore) = _userContexts.emplace(userId, userId);
		}

		return itContext->second;
	}

	bool
	FeedbacksSynchronizer::isSyncing() const
	{
		return std::any_of(std::cbegin(_userContexts), std::cend(_userContexts), [](const auto& contextEntry)
				{
					return contextEntry.second.syncing;
				});
	}

	void
	FeedbacksSynchronizer::scheduleSync(std::chrono::seconds fromNow)
	{
		if (_syncFeedbacksPeriod.count() == 0 || _maxSyncFeedbackCount == 0)
			return;

		LOG(DEBUG) << "Scheduled sync in " << fromNow.count() << " seconds...";
		_syncTimer.expires_after(fromNow);
		_syncTimer.async_wait(boost::asio::bind_executor(_strand, [this] (const boost::system::error_code& ec)
		{
			if (ec == boost::asio::error::operation_aborted)
			{
				LOG(DEBUG) << "getFeedbacks aborted";
				return;
			}
			else if (ec)
			{
				throw Exception {"GetFeedbacks timer failure: " + std::string {ec.message()} };
			}

			startSync();
		}));
	}

	void
	FeedbacksSynchronizer::startSync()
	{
		LOG(DEBUG) << "Starting sync!";

		assert(!isSyncing());
		assert(_strand.running_in_this_thread());

		enquePendingFeedbacks();

		Database::RangeResults<Database::UserId> userIds;
		{
			Database::Session& session {_db.getTLSSession()};
			auto transaction {session.createSharedTransaction()};
			userIds = Database::User::find(_db.getTLSSession(), Database::User::FindParameters{}.setScrobbler(Database::Scrobbler::ListenBrainz));
		}

		for (const Database::UserId userId : userIds.results)
			startSync(getUserContext(userId));

		if (!isSyncing())
			scheduleSync(_syncFeedbacksPeriod);
	}

	void
	FeedbacksSynchronizer::startSync(UserContext& context)
	{
		context.syncing = true;
		context.listenBrainzUserName = "";
		context.fetchedFeedbackCount = 0;
		context.matchedFeedbackCount = 0;
		context.importedFeedbackCount = 0;

		enqueValidateToken(context);
	}

	void
	FeedbacksSynchronizer::onSyncEnded(UserContext& context)
	{
		_strand.dispatch([this, &context]
		{
			LOG(INFO) << "Feedback sync done for user '" << context.listenBrainzUserName << "', fetched: " << context.fetchedFeedbackCount << ", matched: " << context.matchedFeedbackCount << ", imported: " << context.importedFeedbackCount;
			context.syncing = false;

			if (!isSyncing())
				scheduleSync(_syncFeedbacksPeriod);
		});
	}

	void
	FeedbacksSynchronizer::enqueValidateToken(UserContext& context)
	{
		assert(context.listenBrainzUserName.empty());

		const std::optional<UUID> listenBrainzToken {Utils::getListenBrainzToken(_db.getTLSSession(), context.userId)};
		if (!listenBrainzToken)
		{
			onSyncEnded(context);
			return;
		}

		Http::ClientGETRequestParameters request;
		request.priority = Http::ClientRequestParameters::Priority::Low;
		request.relativeUrl = "/1/validate-token";
		request.headers = { {"Authorization",  "Token " + std::string {listenBrainzToken->getAsString()}} };
		request.onSuccessFunc = [this, &context] (std::string_view msgBody)
			{
				context.listenBrainzUserName = Utils::parseValidateToken(msgBody);
				if (context.listenBrainzUserName.empty())
				{
					onSyncEnded(context);
					return;
				}
				enqueGetFeedbackCount(context);
			};
		request.onFailureFunc = [this, &context]
			{
				onSyncEnded(context);
			};

		_client.sendGETRequest(std::move(request));
	}

	void
	FeedbacksSynchronizer::enqueGetFeedbackCount(UserContext& context)
	{
		assert(!context.listenBrainzUserName.empty());

		Http::ClientGETRequestParameters request;
		request.relativeUrl = "/1/feedback/user/" + std::string {context.listenBrainzUserName} + "/get-feedback?score=1&count=0";
		request.priority = Http::ClientRequestParameters::Priority::Low;
		request.onSuccessFunc = [this, &context] (std::string_view msgBody)
			{
				std::string msgBodyCopy {msgBody};
				_strand.dispatch([this, msgBodyCopy, &context]
				{
					LOG(DEBUG) << "Current feedback count = " << (context.feedbackCount ? *context.feedbackCount : 0) << " for user '" << context.listenBrainzUserName << "'";

					const auto totalFeedbackCount = parseTotalFeedbackCount(msgBodyCopy);
					if (totalFeedbackCount)
						LOG(DEBUG) << "Feedback count for listenbrainz user '" << context.listenBrainzUserName << "' = " << *totalFeedbackCount;

					bool needSync {totalFeedbackCount && (!context.feedbackCount || *context.feedbackCount != *totalFeedbackCount)};
					context.feedbackCount = totalFeedbackCount;

					if (needSync)
						enqueGetFeedbacks(context);
					else
						onSyncEnded(context);
				});
			};
		request.onFailureFunc = [this, &context]
			{
				onSyncEnded(context);
			};

		_client.sendGETRequest(std::move(request));
	}

	void
	FeedbacksSynchronizer::enqueGetFeedbacks(UserContext& context)
	{
		assert(!context.listenBrainzUserName.empty());

		Http::ClientGETRequestParameters request;
		request.relativeUrl = "/1/feedback/user/" + context.listenBrainzUserName + "/get-feedback?offset=" + std::to_string(context.fetchedFeedbackCount);
		request.priority = Http::ClientRequestParameters::Priority::Low;
		request.onSuccessFunc = [this, &context] (std::string_view msgBody)
			{
				std::string msgBodyCopy {msgBody};
				_strand.dispatch([this, msgBodyCopy, &context]
				{
					const std::size_t fetchedFeedbackCount {processGetFeedbacks(msgBodyCopy, context)};
					if (fetchedFeedbackCount == 0 // no more thing available on server
							|| context.fetchedFeedbackCount >= context.feedbackCount // we may miss something, but we will get it next time
							|| context.fetchedFeedbackCount >= _maxSyncFeedbackCount)
					{
						onSyncEnded(context);
					}
					else
					{
						enqueGetFeedbacks(context);
					}
				});
			};
		request.onFailureFunc = [=, &context]
			{
				onSyncEnded(context);
			};

		_client.sendGETRequest(std::move(request));
	}

	std::size_t
	FeedbacksSynchronizer::processGetFeedbacks(std::string_view msgBody, UserContext& context)
	{
		const FeedbacksParser::Result parseResult {FeedbacksParser::parse(msgBody)};

		LOG(DEBUG) << "Parsed " << parseResult.feedbackCount << " feedbacks, found " << parseResult.feedbacks.size() << " usable entries";
		context.fetchedFeedbackCount += parseResult.feedbackCount;

		for (const Feedback& feedback : parseResult.feedbacks)
		{
			tryImportFeedback(feedback, context);
		}

		return parseResult.feedbackCount;
	}

	void
	FeedbacksSynchronizer::tryImportFeedback(const Feedback& feedback, UserContext& context)
	{
		Database::Session& session {_db.getTLSSession()};

		bool needImport{};
		TrackId trackId;

		{
			auto transaction {session.createSharedTransaction()};
			const std::vector<Track::pointer> tracks {Track::findByRecordingMBID(session, feedback.recordingMBID)};
			if (tracks.size() > 1)
			{
				LOG(DEBUG) << "Too many matches for feedback '" << feedback << "': duplicate recording MBIDs found";
				return;
			}
			else if (tracks.empty())
			{
				LOG(DEBUG) << "Cannot match feedback '" << feedback << "': no track found for this recording MBID";
				return;
			}

			trackId = tracks.front()->getId();

			const StarredTrack::pointer starredTrack {StarredTrack::find(session, trackId, context.userId, Database::Scrobbler::ListenBrainz)};
			needImport = !starredTrack;

			// don't update starred date time
			// no need to update state if it was found as not synchronized
			// pending remove => will be removed later
			// pending add => will be resent later
		}

		if (needImport)
		{
			LOG(DEBUG) << "Importing feedback '" << feedback << "'";

			auto transaction {session.createUniqueTransaction()};

			const Track::pointer track {Track::find(session, trackId)};
			if (!track)
				return;

			const User::pointer user {User::find(session, context.userId)};
			if (!user)
				return;

			StarredTrack::pointer starredTrack {session.create<StarredTrack>(track, user, Database::Scrobbler::ListenBrainz)};
			starredTrack.modify()->setScrobblingState(ScrobblingState::Synchronized);
			starredTrack.modify()->setDateTime(feedback.created);

			context.importedFeedbackCount++;
		}
		else
		{
			LOG(DEBUG) << "No need to import feedback '" << feedback << "', already imported";
			context.matchedFeedbackCount++;
		}
	}
} // namespace Scrobbling::ListenBrainz
