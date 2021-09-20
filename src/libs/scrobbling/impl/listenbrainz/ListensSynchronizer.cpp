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

#include <boost/asio/bind_executor.hpp>
#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Serializer.h>

#include "database/Artist.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"
#include "scrobbling/Exception.hpp"
#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

#include "Utils.hpp"

#define LOG(sev)	LMS_LOG(SCROBBLING, sev) << "[listenbrainz Synchronizer] - "

namespace
{
	using namespace Scrobbling::ListenBrainz;

	SendQueue::RequestData
	createValidateTokenRequestData(std::string_view authToken)
	{
		SendQueue::RequestData requestData;
		requestData.type = SendQueue::RequestData::Type::GET;
		requestData.endpoint = "/1/validate-token";
		requestData.headers = { {"Authorization",  "Token " + std::string {authToken}} };

		return requestData;
	}

	std::string
	parseValidateToken(std::string_view msgBody)
	{
		std::string listenBrainzUserName;

		Wt::Json::ParseError error;
		Wt::Json::Object root;
		if (!Wt::Json::parse(std::string {msgBody}, root, error))
		{
			LOG(ERROR) << "Cannot parse 'validate-token' result: " << error.what();
			return listenBrainzUserName;
		}

		if (!root.get("valid").orIfNull(false))
		{
			LOG(INFO) << "Invalid listenbrainz user";
			return listenBrainzUserName;
		}

		listenBrainzUserName = root.get("user_name").orIfNull("");
		return listenBrainzUserName;
	}

	SendQueue::RequestData
	createListenCountRequestData(std::string_view listenBrainzUserName)
	{
		LOG(DEBUG) << "Getting listen count for listenbrainz user '" << listenBrainzUserName << "'";

		SendQueue::RequestData requestData;
		requestData.type = SendQueue::RequestData::Type::GET;
		requestData.endpoint = "/1/user/" + std::string {listenBrainzUserName} + "/listen-count";

		return requestData;
	}

	std::optional<std::size_t>
	parseListenCount(std::string_view msgBody)
	{
		try
		{
			Wt::Json::Object root;
			Wt::Json::parse(std::string {msgBody}, root);

			const Wt::Json::Object& payload {static_cast<const Wt::Json::Object&>(root.get("payload"))};
			return static_cast<int>(payload.get("count"));
		}
		catch (const Wt::WException& e)
		{
			LOG(ERROR) << "Cannot parse listen count response: " << e.what();
			return std::nullopt;
		}
	}

	SendQueue::RequestData
	createGetListensRequestData(std::string_view listenBrainzUserName, const Wt::WDateTime& maxDateTime)
	{
		LOG(DEBUG) << "Getting listens for listenbrainz user '" << listenBrainzUserName << "' with max_ts = " << maxDateTime.toString();

		SendQueue::RequestData requestData;
		requestData.type = SendQueue::RequestData::Type::GET;
		requestData.endpoint = "/1/user/" + std::string {listenBrainzUserName} + "/listens?max_ts=" + std::to_string(maxDateTime.toTime_t());

		return requestData;
	}

	Database::Track::pointer
	tryMatchListen(Database::Session& session, const Wt::Json::Object& metadata)
	{
		Database::Track::pointer track;

		// first try to get the associated track using MBIDs, and then fallback on names
		if (metadata.type("additional_info") == Wt::Json::Type::Object)
		{
			const Wt::Json::Object& additionalInfo = metadata.get("additional_info");
			if (std::optional<UUID> recordingMBID {UUID::fromString(additionalInfo.get("recording_mbid").orIfNull(""))})
			{
				const auto tracks {Database::Track::getByRecordingMBID(session, *recordingMBID)};
				// if duplicated files, do not record it (let the user correct its database)
				if (tracks.size() == 1)
					track = tracks.front();
			}
		}

		if (track)
			return track;

		// these fields are mandatory
		const std::string trackName {static_cast<std::string>(metadata.get("track_name"))};
		const std::string releaseName {static_cast<std::string>(metadata.get("release_name"))};

		auto tracks {Database::Track::getByNameAndReleaseName(session, trackName, releaseName)};
		if (tracks.size() > 1)
		{
			tracks.erase(std::remove_if(std::begin(tracks), std::end(tracks),
				[&](const Database::Track::pointer track)
				{
					if (std::string artistName {metadata.get("artist_name").orIfNull("")}; !artistName.empty())
					{
						const auto& artists {track->getArtists({Database::TrackArtistLinkType::Artist})};
						if (std::none_of(std::begin(artists), std::end(artists), [&](const Database::Artist::pointer& artist) { return artist->getName() == artistName; }))
							return true;
					}
					if (metadata.type("additional_info") == Wt::Json::Type::Object)
					{
						const Wt::Json::Object& additionalInfo = metadata.get("additional_info");
						if (track->getTrackNumber())
						{
							int otherTrackNumber {additionalInfo.get("tracknumber").orIfNull(-1)};
							if (otherTrackNumber > 0 && static_cast<std::size_t>(otherTrackNumber) != *track->getTrackNumber())
								return true;
						}

						if (auto releaseMBID {track->getRelease()->getMBID()})
						{
							if (std::optional<UUID> otherReleaseMBID {UUID::fromString(additionalInfo.get("release_mbid").orIfNull(""))})
							{
								if (otherReleaseMBID->getAsString() != releaseMBID->getAsString())
									return true;
							}
						}
					}

					return false;
				}), std::end(tracks));
		}

		if (tracks.size() == 1)
			track = tracks.front();

		return track;
	}

	struct ParseGetListensResult
	{
		Wt::WDateTime oldestEntry;
		std::size_t listenCount{};
		std::vector<Scrobbling::TimedListen> matchedListens;
	};
	ParseGetListensResult
	parseGetListens(Database::Session& session, std::string_view msgBody, Database::UserId userId)
	{
		ParseGetListensResult result;

		try
		{
			Wt::Json::Object root;
			Wt::Json::parse(std::string {msgBody}, root);

			const Wt::Json::Object& payload = root.get("payload");
			const Wt::Json::Array& listens = payload.get("listens");

			LOG(DEBUG) << "Got " << listens.size() << " listens";

			if (listens.empty())
				return result;

			auto transaction {session.createSharedTransaction()};

			for (const Wt::Json::Value& value : listens)
			{
				const Wt::Json::Object& listen = value;
				const Wt::WDateTime listenedAt {Wt::WDateTime::fromTime_t(static_cast<int>(listen.get("listened_at")))};
				const Wt::Json::Object& metadata = listen.get("track_metadata");

				if (!listenedAt.isValid())
				{
					LOG(ERROR) << "bad listened_at field!";
					continue;
				}

				result.listenCount++;
				if (!result.oldestEntry.isValid())
					result.oldestEntry = listenedAt;
				else if (listenedAt < result.oldestEntry)
					result.oldestEntry = listenedAt;

				if (const Database::Track::pointer track {tryMatchListen(session, metadata)})
					result.matchedListens.emplace_back(Scrobbling::TimedListen {userId, track->getId(), listenedAt});
			}
		}
		catch (const Wt::WException& error)
		{
			LOG(ERROR) << "Cannot parse 'get-listens' result: " << error.what();
		}

		return result;
	}
}

namespace Scrobbling::ListenBrainz
{
	ListensSynchronizer::ListensSynchronizer(boost::asio::io_context& ioContext, Database::Db& db, SendQueue& sendQueue)
		: _ioContext {ioContext}
		, _db {db}
		, _sendQueue {sendQueue}
		, _maxSyncListenCount {Service<IConfig>::get()->getULong("listenbrainz-max-sync-listen-count", 1000)}
		, _syncListensPeriod {Service<IConfig>::get()->getULong("listenbrainz-sync-listens-period-hours", 1)}
	{
		LOG(INFO) << "Starting Listens synchronizer, maxSyncListenCount = " << _maxSyncListenCount << ", _syncListensPeriod = " << _syncListensPeriod.count() << " hours";

		scheduleGetListens(std::chrono::seconds {30});
	}

	void
	ListensSynchronizer::saveListen(const TimedListen& listen)
	{
		_strand.dispatch([=]
		{
			Database::Session& session {_db.getTLSSession()};

			auto transaction {session.createUniqueTransaction()};

			const Database::User::pointer user {Database::User::getById(session, listen.userId)};
			if (!user)
				return;

			const Database::Track::pointer track {Database::Track::getById(session, listen.trackId)};
			if (!track)
				return;

			Database::TrackListEntry::create(session, track, Utils::getOrCreateListensTrackList(session, user), listen.listenedAt);

			UserContext& context {getUserContext(listen.userId)};
			if (context.listenCount)
				(*context.listenCount)++;
		});
	}

	ListensSynchronizer::UserContext&
	ListensSynchronizer::getUserContext(Database::UserId userId)
	{
		auto itContext {_userContexts.find(userId)};
		if (itContext == std::cend(_userContexts))
		{
			auto [itNewContext, inserted] {_userContexts.emplace(userId, userId)};
			itContext = itNewContext;
		}

		return itContext->second;
	}

	bool
	ListensSynchronizer::isFetching() const
	{
		return std::any_of(std::cbegin(_userContexts), std::cend(_userContexts), [](const auto& contextEntry)
				{
					const auto& [userId, context] {contextEntry};
					return context.fetching;
				});
	}

	void
	ListensSynchronizer::scheduleGetListens(std::chrono::seconds fromNow)
	{
		if (_syncListensPeriod.count() == 0 || _maxSyncListenCount == 0)
			return;

		LOG(DEBUG) << "Scheduled sync in " << fromNow.count() << " seconds...";
		_getListensTimer.expires_after(fromNow);
		_getListensTimer.async_wait(boost::asio::bind_executor(_strand, [this] (const boost::system::error_code& ec)
		{
			if (ec == boost::asio::error::operation_aborted)
			{
				LOG(DEBUG) << "getListens aborted";
				return;
			}
			else if (ec)
			{
				throw Exception {"GetListens timer failure: " + std::string {ec.message()} };
			}

			startGetListens();
		}));
	}

	void
	ListensSynchronizer::startGetListens()
	{
		LOG(DEBUG) << "GetListens started!!!";

		assert(!isFetching());

		std::vector<Database::UserId> userIds;
		{
			Database::Session& session {_db.getTLSSession()};
			auto transaction {session.createSharedTransaction()};
			userIds = Database::User::getAllIds(_db.getTLSSession());
		}

		for (const Database::UserId userId : userIds)
		{
			if (Utils::getListenBrainzToken(_db.getTLSSession(), userId))
				startGetListens(getUserContext(userId));
		}

		if (!isFetching())
			scheduleGetListens(_syncListensPeriod);
	}

	void
	ListensSynchronizer::startGetListens(UserContext& context)
	{
		context.fetching = true;
		context.listenBrainzUserName = "";
		context.maxDateTime = {};
		context.fetchedListenCount = 0;
		context.matchedListenCount = 0;
		context.importedListenCount = 0;

		enqueValidateToken(context);
	}

	void
	ListensSynchronizer::onGetListensEnded(UserContext& context)
	{
		_strand.dispatch([this, &context]
		{
			LOG(DEBUG) << "Fetch done for user " << context.userId.getValue() << ", fetched: " << context.fetchedListenCount << ", matched: " << context.matchedListenCount << ", imported: " << context.importedListenCount;
			context.fetching = false;

			if (!isFetching())
				scheduleGetListens(_syncListensPeriod);
		});
	}

	void
	ListensSynchronizer::enqueValidateToken(UserContext& context)
	{
		assert(context.listenBrainzUserName.empty());

		std::optional<SendQueue::RequestData> requestData {createValidateTokenRequestData(context.userId)};
		if (!requestData)
		{
			onGetListensEnded(context);
			return;
		}

		SendQueue::Request validateTokenRequest {std::move(*requestData)};
		validateTokenRequest.setOnSuccessFunc([this, &context] (std::string_view msgBody)
		{
			context.listenBrainzUserName = parseValidateToken(msgBody);
			if (context.listenBrainzUserName.empty())
			{
				onGetListensEnded(context);
				return;
			}
			enqueGetListenCount(context);
		});
		validateTokenRequest.setOnFailureFunc([this, &context]
		{
			onGetListensEnded(context);
		});

		validateTokenRequest.setPriority(SendQueue::Request::Priority::Low);
		_sendQueue.enqueueRequest(std::move(validateTokenRequest));
	}

	void
	ListensSynchronizer::enqueGetListenCount(UserContext& context)
	{
		assert(!context.listenBrainzUserName.empty());

		SendQueue::Request getListenCountRequest {createListenCountRequestData(context.listenBrainzUserName)};
		getListenCountRequest.setOnSuccessFunc([=, &context] (std::string_view msgBody)
		{
			const auto listenCount = parseListenCount(msgBody);
			if (listenCount)
				LOG(DEBUG) << "Listen count for listenbrainz user '" << context.listenBrainzUserName << "' = " << *listenCount;

			bool needSync {listenCount && (!context.listenCount || *context.listenCount != *listenCount)};
			context.listenCount = listenCount;

			if (!needSync)
			{
				onGetListensEnded(context);
				return;
			}

			context.maxDateTime = Wt::WDateTime::currentDateTime();
			enqueGetListens(context);
		});
		getListenCountRequest.setOnFailureFunc([this, &context]
		{
			onGetListensEnded(context);
		});

		getListenCountRequest.setPriority(SendQueue::Request::Priority::Low);
		_sendQueue.enqueueRequest(std::move(getListenCountRequest));
	}

	void
	ListensSynchronizer::enqueGetListens(UserContext& context)
	{
		assert(!context.listenBrainzUserName.empty());

		SendQueue::Request getListensRequest {::createGetListensRequestData(context.listenBrainzUserName, context.maxDateTime)};
		getListensRequest.setOnSuccessFunc([=, &context] (std::string_view msgBody)
		{
			processGetListensResponse(msgBody, context);
			if (context.fetchedListenCount >= _maxSyncListenCount || !context.maxDateTime.isValid())
			{
				onGetListensEnded(context);
				return;
			}

			enqueGetListens(context);
		});
		getListensRequest.setOnFailureFunc([=, &context]
		{
			onGetListensEnded(context);
		});

		getListensRequest.setPriority(SendQueue::Request::Priority::Low);
		_sendQueue.enqueueRequest(std::move(getListensRequest));
	}

	std::optional<SendQueue::RequestData>
	ListensSynchronizer::createValidateTokenRequestData(Database::UserId userId)
	{
		Database::Session& session {_db.getTLSSession()};

		const std::optional<UUID> listenBrainzToken {Utils::getListenBrainzToken(session, userId)};
		if (!listenBrainzToken)
			return std::nullopt;

		return ::createValidateTokenRequestData(listenBrainzToken->getAsString());
	}

	void
	ListensSynchronizer::processGetListensResponse(std::string_view msgBody, UserContext& context)
	{
		Database::Session& session {_db.getTLSSession()};

		const ParseGetListensResult parseResult {parseGetListens(session, msgBody, context.userId)};

		context.fetchedListenCount += parseResult.listenCount;
		context.matchedListenCount += parseResult.matchedListens.size();
		context.maxDateTime = parseResult.oldestEntry;

		if (parseResult.matchedListens.empty())
			return;

		auto transaction {session.createUniqueTransaction()};

		Database::User::pointer user {Database::User::getById(session, context.userId)};
		if (!user)
			return;

		Database::TrackList::pointer tracklist {Utils::getOrCreateListensTrackList(session, user)};

		for (const TimedListen& listen : parseResult.matchedListens)
		{
			const Database::Track::pointer track {Database::Track::getById(session, listen.trackId)};
			if (!track)
				continue;

			if (!tracklist->getEntryByTrackAndDateTime(track, listen.listenedAt))
			{
				context.importedListenCount++;
				Database::TrackListEntry::create(session, track, tracklist, listen.listenedAt);
			}
		}
	}

} // namespace Scrobbling::ListenBrainz

