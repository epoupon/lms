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

#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "utils/IConfig.hpp"
#include "utils/http/IClient.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "Utils.hpp"

using namespace Database;

namespace
{
	bool
	canBeScrobbled(Session& session, TrackId trackId, std::chrono::seconds duration)
	{
		auto transaction {session.createSharedTransaction()};

		const Track::pointer track {Track::find(session, trackId)};
		if (!track)
			return false;

		const bool res {duration >= std::chrono::minutes(4) || (duration >= track->getDuration() / 2)};
		if (!res)
			LOG(DEBUG) << "Track cannot be scrobbled since played duration is too short: " << duration.count() << "s, total duration = " << std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count() << "s";

		return res;
	}

	std::optional<Wt::Json::Object>
	listenToJsonPayload(Session& session, const Scrobbling::Listen& listen, const Wt::WDateTime& timePoint)
	{
		auto transaction {session.createSharedTransaction()};

		const Track::pointer track {Track::find(session, listen.trackId)};
		if (!track)
			return std::nullopt;

		auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		if (artists.empty())
			artists = track->getArtists({TrackArtistLinkType::ReleaseArtist});

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
			for (const Artist::pointer& artist : artists)
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
	listenToJsonString(Session& session, const Scrobbling::Listen& listen, const Wt::WDateTime& timePoint, std::string_view listenType)
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
	Scrobbler::Scrobbler(boost::asio::io_context& ioContext, Db& db)
		: _ioContext {ioContext}
		, _db {db}
		, _baseAPIUrl {Service<IConfig>::get()->getString("listenbrainz-api-base-url", "https://api.listenbrainz.org")}
		, _client {Http::createClient(_ioContext, _baseAPIUrl)}
		, _listensSynchronizer {_ioContext, db, *_client}
		, _feedbackSender {db, *_client}
	{
		LOG(INFO) << "Starting ListenBrainz scrobbler... API endpoint = '" << _baseAPIUrl;
	}

	Scrobbler::~Scrobbler()
	{
		LOG(INFO) << "Stopped ListenBrainz scrobbler!";
	}

	void
	Scrobbler::listenStarted(const Listen& listen)
	{
		_listensSynchronizer.enqueListenNow(listen);
	}

	void
	Scrobbler::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
	{
		if (duration && !canBeScrobbled(_db.getTLSSession(), listen.trackId, *duration))
			return;

		const TimedListen timedListen {listen, Wt::WDateTime::currentDateTime()};
		_listensSynchronizer.enqueListen(timedListen);
	}

	void
	Scrobbler::addTimedListen(const TimedListen& timedListen)
	{
		_listensSynchronizer.enqueListen(timedListen);
	}

	void
	Scrobbler::onStarred(UserId userId, TrackId trackId)
	{
		const Feedback feedback {Feedback::Type::Love, userId, trackId};
		_feedbackSender.enqueFeedback(feedback);
	}

	void
	Scrobbler::onUnstarred(UserId userId, TrackId trackId)
	{
		const Feedback feedback {Feedback::Type::Erase, userId, trackId};
		_feedbackSender.enqueFeedback(feedback);
	}
} // namespace Scrobbling::ListenBrainz
