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

#include "services/database/Artist.hpp"
#include "services/database/Db.hpp"
#include "services/database/Listen.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/User.hpp"
#include "services/scrobbling/Exception.hpp"
#include "utils/IConfig.hpp"
#include "utils/http/IClient.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

#include "Utils.hpp"

#define LOG(sev)	LMS_LOG(SCROBBLING, sev) << "[listenbrainz Synchronizer] - "

namespace
{
	using namespace Scrobbling::ListenBrainz;

	std::optional<Wt::Json::Object>
	listenToJsonPayload(Database::Session& session, const Scrobbling::Listen& listen, const Wt::WDateTime& timePoint)
	{
		auto transaction {session.createSharedTransaction()};

		const Database::Track::pointer track {Database::Track::find(session, listen.trackId)};
		if (!track)
			return std::nullopt;

		auto artists {track->getArtists({Database::TrackArtistLinkType::Artist})};
		if (artists.empty())
			artists = track->getArtists({Database::TrackArtistLinkType::ReleaseArtist});

		if (artists.empty())
		{
			LOG(DEBUG) << "Track cannot be scrobbled since it does not have any artist";
			return std::nullopt;
		}

		Wt::Json::Object additionalInfo;
		additionalInfo["listening_from"] = "LMS";
		if (track->getRelease())
		{
			if (auto MBID {track->getRelease()->getMBID()})
				additionalInfo["release_mbid"] = Wt::Json::Value {std::string {MBID->getAsString()}};
		}

		{
			Wt::Json::Array artistMBIDs;
			for (const Database::Artist::pointer& artist : artists)
			{
				if (auto MBID {artist->getMBID()})
					artistMBIDs.push_back(Wt::Json::Value {std::string {MBID->getAsString()}});
			}

			if (!artistMBIDs.empty())
				additionalInfo["artist_mbids"] = std::move(artistMBIDs);
		}

		if (auto MBID {track->getTrackMBID()})
			additionalInfo["track_mbid"] = Wt::Json::Value {std::string {MBID->getAsString()}};

		if (auto MBID {track->getRecordingMBID()})
			additionalInfo["recording_mbid"] = Wt::Json::Value {std::string {MBID->getAsString()}};

		if (const std::optional<std::size_t> trackNumber {track->getTrackNumber()})
			additionalInfo["tracknumber"] = Wt::Json::Value {static_cast<long long int>(*trackNumber)};

		Wt::Json::Object trackMetadata;
		trackMetadata["additional_info"] = std::move(additionalInfo);
		trackMetadata["artist_name"] = Wt::Json::Value {artists.front()->getName()};
		trackMetadata["track_name"] = Wt::Json::Value {track->getName()};
		if (track->getRelease())
			trackMetadata["release_name"] = Wt::Json::Value {track->getRelease()->getName()};

		Wt::Json::Object payload;
		payload["track_metadata"] = std::move(trackMetadata);
		if (timePoint.isValid())
			payload["listened_at"] = Wt::Json::Value {static_cast<long long int>(timePoint.toTime_t())};

		return payload;
	}

	std::string
	listenToJsonString(Database::Session& session, const Scrobbling::Listen& listen, const Wt::WDateTime& timePoint, std::string_view listenType)
	{
		std::string res;

		std::optional<Wt::Json::Object> payload {listenToJsonPayload(session, listen, timePoint)};
		if (!payload)
			return res;

		Wt::Json::Object root;
		root["listen_type"] = Wt::Json::Value {std::string {listenType}};
		root["payload"] = Wt::Json::Array {std::move(*payload)};

		res = Wt::Json::serialize(root);
		return res;
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

	Database::TrackId
	tryMatchListen(Database::Session& session, const Wt::Json::Object& metadata)
	{
		using namespace Database;

		// first try to get the associated track using MBIDs, and then fallback on names
		if (metadata.type("additional_info") == Wt::Json::Type::Object)
		{
			const Wt::Json::Object& additionalInfo = metadata.get("additional_info");
			if (std::optional<UUID> recordingMBID {UUID::fromString(additionalInfo.get("recording_mbid").orIfNull(""))})
			{
				const auto tracks {Track::findByRecordingMBID(session, *recordingMBID)};
				// if duplicated files, do not record it (let the user correct its database)
				if (tracks.size() == 1)
					return tracks.front()->getId();
			}
		}

		// these fields are mandatory
		const std::string trackName {static_cast<std::string>(metadata.get("track_name"))};
		const std::string releaseName {static_cast<std::string>(metadata.get("release_name"))};

		auto tracks {Track::findByNameAndReleaseName(session, trackName, releaseName)};
		if (tracks.results.size() > 1)
		{
			tracks.results.erase(std::remove_if(std::begin(tracks.results), std::end(tracks.results),
				[&](const TrackId trackId)
				{
					const Track::pointer track {Track::find(session, trackId)};

					if (std::string artistName {metadata.get("artist_name").orIfNull("")}; !artistName.empty())
					{
						const auto& artists {track->getArtists({TrackArtistLinkType::Artist})};
						if (std::none_of(std::begin(artists), std::end(artists), [&](const Artist::pointer& artist) { return artist->getName() == artistName; }))
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
				}), std::end(tracks.results));
		}

		if (tracks.results.size() == 1)
			return tracks.results.front();

		return {};
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

				if (Database::TrackId trackId {tryMatchListen(session, metadata)}; trackId.isValid())
					result.matchedListens.emplace_back(Scrobbling::TimedListen {{userId, trackId}, listenedAt});
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
	ListensSynchronizer::ListensSynchronizer(boost::asio::io_context& ioContext, Database::Db& db, Http::IClient& client)
		: _ioContext {ioContext}
		, _db {db}
		, _client {client}
		, _maxSyncListenCount {Service<IConfig>::get()->getULong("listenbrainz-max-sync-listen-count", 1000)}
		, _syncListensPeriod {Service<IConfig>::get()->getULong("listenbrainz-sync-listens-period-hours", 1)}
	{
		LOG(INFO) << "Starting Listens synchronizer, maxSyncListenCount = " << _maxSyncListenCount << ", _syncListensPeriod = " << _syncListensPeriod.count() << " hours";

		scheduleGetListens(std::chrono::seconds {30});
	}

	void
	ListensSynchronizer::enqueListen(const Listen& listen, const Wt::WDateTime& timePoint)
	{
		Http::ClientPOSTRequestParameters request;
		request.relativeUrl = "/1/submit-listens";

		if (timePoint.isValid())
		{
			// We want the listen to be sent again later in case of failure, so we just save it as pending send
			const Database::ListenId listenId {saveListen(TimedListen {listen, timePoint})};
			if (!listenId.isValid())
				return;

			request.priority = Http::ClientRequestParameters::Priority::Normal;
			request.onSuccessFunc = [=](std::string_view)
			{
				onListenSent(listenId);
			};
			// on failure, this listen will be sent during the next sync
		}
		else
		{
			// We want "listen now" to appear as soon as possible
			request.priority = Http::ClientRequestParameters::Priority::High;
			// don't retry on failure
		}

		std::string bodyText {listenToJsonString(_db.getTLSSession(), listen, timePoint, timePoint.isValid() ? "single" : "playing_now")};
		if (bodyText.empty())
		{
			LOG(DEBUG) << "Cannot convert listen to json: skipping";
			return;
		}

		const std::optional<UUID> listenBrainzToken {Utils::getListenBrainzToken(_db.getTLSSession(), listen.userId)};
		if (!listenBrainzToken)
			return;

		request.message.addBodyText(bodyText);
		request.message.addHeader("Authorization", "Token " + std::string {listenBrainzToken->getAsString()});
		request.message.addHeader("Content-Type", "application/json");
		_client.sendPOSTRequest(std::move(request));

	}

	Database::ListenId
	ListensSynchronizer::saveListen(const TimedListen& listen)
	{
		using namespace Database;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createUniqueTransaction()};

		if (Database::Listen::find(session, listen.userId, listen.trackId, Database::Scrobbler::ListenBrainz, listen.listenedAt))
			return {};

		const User::pointer user {User::find(session, listen.userId)};
		if (!user)
			return {};

		const Track::pointer track {Track::find(session, listen.trackId)};
		if (!track)
			return {};

		const auto dbListen {Database::Listen::create(session, user, track, Database::Scrobbler::ListenBrainz, listen.listenedAt)};
		assert(dbListen->getScrobblingState() == Database::ScrobblingState::PendingAdd);

		return dbListen->getId();
	}

	void
	ListensSynchronizer::onListenSent(Database::ListenId listenId)
	{
		_strand.dispatch([=]
		{
			Database::Session& session {_db.getTLSSession()};
			auto transaction {session.createUniqueTransaction()};

			if (Database::Listen::pointer listen {Database::Listen::find(session, listenId)})
			{
				listen.modify()->setScrobblingState(Database::ScrobblingState::Synchronized);

				UserContext& context {getUserContext(listen->getUser()->getId())};
				if (context.listenCount)
					(*context.listenCount)++;
			}
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
		LOG(DEBUG) << "GetListens started!";

		assert(!isFetching());

		Database::RangeResults<Database::UserId> userIds;
		{
			Database::Session& session {_db.getTLSSession()};
			auto transaction {session.createSharedTransaction()};
			userIds = Database::User::find(_db.getTLSSession(), Database::Range {});
		}

		for (const Database::UserId userId : userIds.results)
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

		const std::optional<UUID> listenBrainzToken {Utils::getListenBrainzToken(_db.getTLSSession(), context.userId)};
		if (!listenBrainzToken)
		{
			onGetListensEnded(context);
			return;
		}

		Http::ClientGETRequestParameters request;
		request.priority = Http::ClientRequestParameters::Priority::Low;
		request.relativeUrl = "/1/validate-token";
		request.headers = { {"Authorization",  "Token " + std::string {listenBrainzToken->getAsString()}} };
		request.onSuccessFunc = [this, &context] (std::string_view msgBody)
			{
				context.listenBrainzUserName = parseValidateToken(msgBody);
				if (context.listenBrainzUserName.empty())
				{
					onGetListensEnded(context);
					return;
				}
				enqueGetListenCount(context);
			};
		request.onFailureFunc = [this, &context]
			{
				onGetListensEnded(context);
			};

		_client.sendGETRequest(std::move(request));
	}

	void
	ListensSynchronizer::enqueGetListenCount(UserContext& context)
	{
		assert(!context.listenBrainzUserName.empty());

		Http::ClientGETRequestParameters request;
		request.relativeUrl = "/1/user/" + std::string {context.listenBrainzUserName} + "/listen-count";
		request.priority = Http::ClientRequestParameters::Priority::Low;
		request.onSuccessFunc = [=, &context] (std::string_view msgBody)
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
			};
		request.onFailureFunc = [this, &context]
			{
				onGetListensEnded(context);
			};

		_client.sendGETRequest(std::move(request));
	}

	void
	ListensSynchronizer::enqueGetListens(UserContext& context)
	{
		assert(!context.listenBrainzUserName.empty());

		Http::ClientGETRequestParameters request;
		request.relativeUrl = "/1/user/" + context.listenBrainzUserName + "/listens?max_ts=" + std::to_string(context.maxDateTime.toTime_t());
		request.priority = Http::ClientRequestParameters::Priority::Low;
		request.onSuccessFunc = [=, &context] (std::string_view msgBody)
			{
				processGetListensResponse(msgBody, context);
				if (context.fetchedListenCount >= _maxSyncListenCount || !context.maxDateTime.isValid())
				{
					onGetListensEnded(context);
					return;
				}

				enqueGetListens(context);
			};
		request.onFailureFunc = [=, &context]
			{
				onGetListensEnded(context);
			};

		_client.sendGETRequest(std::move(request));
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

		Database::User::pointer user {Database::User::find(session, context.userId)};
		if (!user)
			return;

		for (const TimedListen& listen : parseResult.matchedListens)
		{
			const Database::Track::pointer track {Database::Track::find(session, listen.trackId)};
			if (!track)
				continue;

			if (Database::Listen::find(session, listen.userId, listen.trackId, Database::Scrobbler::ListenBrainz, listen.listenedAt))
				continue;

			Database::Listen::create(session, user, track, Database::Scrobbler::ListenBrainz, listen.listenedAt);
			context.importedListenCount++;
		}
	}
} // namespace Scrobbling::ListenBrainz

