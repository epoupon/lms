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
#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "Utils.hpp"

#define LOG(sev)	LMS_LOG(SCROBBLING, sev) << "[listenbrainz] - "

namespace
{
	bool
	canBeScrobbled(Database::Session& session, Database::TrackId trackId, std::chrono::seconds duration)
	{
		auto transaction {session.createSharedTransaction()};

		const Database::Track::pointer track {Database::Track::getById(session, trackId)};
		if (!track)
			return false;

		const bool res {duration >= std::chrono::minutes(4) || (duration >= track->getDuration() / 2)};
		if (!res)
			LOG(DEBUG) << "Track cannot be scrobbled since played duration is too short: " << duration.count() << "s, total duration = " << std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count() << "s";

		return res;
	}

	std::optional<Wt::Json::Object>
	listenToJsonPayload(Database::Session& session, const Scrobbling::Listen& listen, const Wt::WDateTime& timePoint)
	{
		auto transaction {session.createSharedTransaction()};

		const Database::Track::pointer track {Database::Track::getById(session, listen.trackId)};
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
}

namespace Scrobbling::ListenBrainz
{
	Scrobbler::Scrobbler(boost::asio::io_context& ioContext, Database::Db& db)
		: _ioContext {ioContext}
		, _db {db}
		, _sendQueue {_ioContext, Service<IConfig>::get()->getString("listenbrainz-api-base-url", "https://api.listenbrainz.org")}
		, _listensSynchronizer {_ioContext, db, _sendQueue}
	{
		LOG(INFO) << "Starting ListenBrainz scrobbler... API endpoint = '" << _sendQueue.getAPIBaseURL();
	}

	Scrobbler::~Scrobbler()
	{
		LOG(INFO) << "Stopped ListenBrainz scrobbler!";
	}

	void
	Scrobbler::listenStarted(const Listen& listen)
	{
		enqueListen(listen, Wt::WDateTime {});
	}

	void
	Scrobbler::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
	{
		if (duration && !canBeScrobbled(_db.getTLSSession(), listen.trackId, *duration))
			return;

		const Listen timedListen {listen};
		const Wt::WDateTime now {Wt::WDateTime::currentDateTime()};

		enqueListen(timedListen, now);
	}

	void
	Scrobbler::addTimedListen(const TimedListen& listen)
	{
		assert(listen.listenedAt.isValid());
		enqueListen(listen, listen.listenedAt);
	}

	Database::TrackList::pointer
	Scrobbler::getListensTrackList(Database::Session& session, Database::User::pointer user)
	{
		return Utils::getListensTrackList(session, user);
	}

	void
	Scrobbler::enqueListen(const Listen& listen, const Wt::WDateTime& timePoint)
	{
		std::optional<SendQueue::RequestData> requestData {createSubmitListenRequestData(listen, timePoint)};
		if (!requestData)
			return;

		SendQueue::Request submitListen {std::move(*requestData)};
		if (timePoint.isValid())
		{
			submitListen.setPriority(SendQueue::Request::Priority::Normal);
			submitListen.setOnSuccessFunc([=](std::string_view)
			{
				_listensSynchronizer.saveListen(TimedListen {listen, timePoint});
			});
		}
		else
		{
			// We want "listen now" to appear as soon as possible
			submitListen.setPriority(SendQueue::Request::Priority::High);
		}

		_sendQueue.enqueueRequest(std::move(submitListen));
	}

	std::optional<SendQueue::RequestData>
	Scrobbler::createSubmitListenRequestData(const Listen& listen, const Wt::WDateTime& timePoint)
	{
		Database::Session& session {_db.getTLSSession()};

		const std::optional<UUID> listenBrainzToken {Utils::getListenBrainzToken(session, listen.userId)};
		if (!listenBrainzToken)
			return std::nullopt;

		SendQueue::RequestData requestData;
		requestData.endpoint = "/1/submit-listens";
		requestData.type = SendQueue::RequestData::Type::POST;

		std::string bodyText {listenToJsonString(session, listen, timePoint, timePoint.isValid() ? "single" : "playing_now")};
		if (bodyText.empty())
		{
			LOG(DEBUG) << "Cannot convert listen to json: skipping";
			return std::nullopt;
		}

		requestData.message.addBodyText(bodyText);
		requestData.message.addHeader("Authorization", "Token " + std::string {listenBrainzToken->getAsString()});
		requestData.message.addHeader("Content-Type", "application/json");

		return requestData;
	}
} // namespace Scrobbling::ListenBrainz

