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

#define LOG(sev)	LMS_LOG(SCROBBLING, sev) << "[listenbrainz] - "

namespace StringUtils
{
	template<>
	std::optional<std::chrono::seconds>
	readAs(const std::string& str)
	{
		std::optional<std::chrono::seconds> res;

		if (const std::optional<std::size_t> value {StringUtils::readAs<std::size_t>(str)})
			res = std::chrono::seconds {*value};

		return res;
	}
}

namespace
{
	std::optional<UUID>
	getListenBrainzToken(Database::Session& session, Database::IdType userId)
	{
		auto transaction {session.createSharedTransaction()};

		const Database::User::pointer user {Database::User::getById(session, userId)};
		if (!user)
			return std::nullopt;

		if (user->getScrobbler() != Database::Scrobbler::ListenBrainz)
			return std::nullopt;

		return user->getListenBrainzToken();
	}

	bool
	canBeScrobbled(Database::Session& session, Database::IdType trackId, std::chrono::seconds duration)
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

	template <typename T>
	std::optional<T>
	headerReadAs(const Wt::Http::Message& msg, std::string_view headerName)
	{
		std::optional<T> res;

		if (const std::string* headerValue {msg.getHeader(std::string {headerName})})
			res = StringUtils::readAs<T>(*headerValue);

		return res;
	}
}

namespace Scrobbling
{
	static const std::string historyTracklistName {"__scrobbler_listenbrainz_history__"};

	ListenBrainzScrobbler::ListenBrainzScrobbler(Database::Db& db)
		: _apiEndpoint {Service<IConfig>::get()->getString("listenbrainz-api-url", "https://api.listenbrainz.org/1/")}
		, _db {db}
	{
		LOG(INFO) << "Starting ListenBrainz scrobbler... API endpoint = '" << _apiEndpoint << "'";

		_client.done().connect([this](Wt::AsioWrapper::error_code ec, Wt::Http::Message msg)
		{
			onClientDone(ec, msg);
		});

		_ioService.setThreadCount(1);
		_ioService.start();
	}

	ListenBrainzScrobbler::~ListenBrainzScrobbler()
	{
		_ioService.stop();

		LOG(INFO) << "Stopped ListenBrainz scrobbler";
	}

	void
	ListenBrainzScrobbler::listenStarted(const Listen& listen)
	{
		_ioService.post([=]
		{
			enqueListen(listen, Wt::WDateTime {});
		});
	}

	void
	ListenBrainzScrobbler::listenFinished(const Listen& listen, std::chrono::seconds duration)
	{
		if (!canBeScrobbled(_db.getTLSSession(), listen.trackId, duration))
			return;

		Listen timedListen {listen};
		const Wt::WDateTime now {Wt::WDateTime::currentDateTime().addSecs(-duration.count())};

		_ioService.post([=]
		{
			enqueListen(timedListen, now);
		});
	}

	void
	ListenBrainzScrobbler::addListen(const Listen& listen, const Wt::WDateTime& timePoint)
	{
		assert(timePoint.isValid());

		_ioService.post([=]
		{
			enqueListen(listen, timePoint);
		});
	}

	Wt::Dbo::ptr<Database::TrackList>
	ListenBrainzScrobbler::getListensTrackList(Database::Session& session, Wt::Dbo::ptr<Database::User> user)
	{
		return Database::TrackList::get(session, historyTracklistName, Database::TrackList::Type::Internal, user);
	}

	void
	ListenBrainzScrobbler::enqueListen(const Listen& listen, const Wt::WDateTime& timePoint)
	{
		if (!timePoint.isValid())
		{
			// If we are currently throttled, just replace the entry if it has no timePoint
			// in order to only report the newest track listened to
			// If not throttled, just search past the next current first message as it is being sent

			const std::size_t offset {_state == State::Throttled ? std::size_t {0} : std::size_t {1}};
			if (_sendQueue.size() > offset)
			{
				_sendQueue.erase(std::remove_if(std::next(std::begin(_sendQueue), offset), std::end(_sendQueue),
							[&](const QueuedListen& queuedListen) { return queuedListen.listen.userId == listen.userId && !queuedListen.timePoint.isValid(); }), std::end(_sendQueue));
			}
		}

		_sendQueue.emplace_back(QueuedListen {listen, timePoint});

		LOG(DEBUG) << "listen queue size = " << _sendQueue.size();

		if (_state == State::Idle)
			sendNextQueuedListen();
	}

	void
	ListenBrainzScrobbler::sendNextQueuedListen()
	{
		assert(_state == State::Idle);

		while (!_sendQueue.empty())
		{
			if (sendListen(_sendQueue.front().listen, _sendQueue.front().timePoint))
			{
				_state = State::Sending;
				break;
			}

			_sendQueue.pop_front();
		}
	}

	bool
	ListenBrainzScrobbler::sendListen(const Listen& listen, const Wt::WDateTime& timePoint)
	{
		Database::Session& session {_db.getTLSSession()};

		const std::optional<UUID> listenBrainzToken {getListenBrainzToken(session, listen.userId)};
		if (!listenBrainzToken)
			return false;

		std::string payload {listenToJsonString(session, listen, timePoint, timePoint.isValid() ? "single" : "playing_now")};
		if (payload.empty())
		{
			LOG(DEBUG) << "Cannot convert listen to json: skipping";
			return false;
		}

		// now send this
		Wt::Http::Message message;
		message.addHeader("Authorization", "Token " + std::string {listenBrainzToken->getAsString()});
		message.addHeader("Content-Type", "application/json");
		message.addBodyText(payload);

		const std::string endPoint {_apiEndpoint + "submit-listens"};
		if (!_client.post(endPoint, message))
		{
			LOG(ERROR) << "Cannot post to '" << endPoint << "': invalid scheme or URL?";
			return false;
		}

		LOG(DEBUG) << "Listen POST done to '" << endPoint << "'";
		return true;
	}

	void
	ListenBrainzScrobbler::onClientDone(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg)
	{
		assert(!_sendQueue.empty());
		QueuedListen& queuedListen {_sendQueue.front()};

		_state = State::Idle;

		LOG(DEBUG) << "POST done. status = " << msg.status() << ", msg = '" << msg.body() << "'";
		if (ec)
		{
			LOG(ERROR) << "Retry " << queuedListen.retryCount << ", client error: '" << ec.message() << "'";
			// may be a network error, try again later
			if (++queuedListen.retryCount > _maxRetryCount)
				_sendQueue.pop_front();

			throttle(_defaultRetryWaitDuration);
			return;
		}

		bool mustThrottle{};

		switch (msg.status())
		{
			case 429:
				mustThrottle = true;
				break;

			case 200:
				if (queuedListen.timePoint.isValid())
					cacheListen(queuedListen.listen, queuedListen.timePoint);
				_sendQueue.pop_front();
				break;

			default:
				LOG(ERROR) << "Submit error: '" << msg.body() << "'";
				_sendQueue.pop_front();
				break;
		}

		const auto remainingCount {headerReadAs<std::size_t>(msg, "X-RateLimit-Remaining")};
		LOG(DEBUG) << "Remaining messages = " << (remainingCount ? *remainingCount : 0);
		if (mustThrottle || (remainingCount && *remainingCount == 0))
		{
			const auto waitDuration {headerReadAs<std::chrono::seconds>(msg, "X-RateLimit-Reset-In")};
			throttle(waitDuration.value_or(_defaultRetryWaitDuration));
		}
		else
		{
			sendNextQueuedListen();
		}
	}

	void
	ListenBrainzScrobbler::throttle(std::chrono::seconds requestedDuration)
	{
		assert(_state == State::Idle);

		const std::chrono::seconds duration {clamp(requestedDuration, _minRetryWaitDuration, _maxRetryWaitDuration)};
		LOG(DEBUG) << "Throttling for " << duration.count() << " seconds";

		_ioService.schedule(duration, [this]
		{
			_state = State::Idle;
			sendNextQueuedListen();
		});
		_state = State::Throttled;
	}

	void
	ListenBrainzScrobbler::cacheListen(const Listen& listen, const Wt::WDateTime& timePoint)
	{
		Database::Session& session {_db.getTLSSession()};

		auto transaction {session.createUniqueTransaction()};

		const Database::User::pointer user {Database::User::getById(session, listen.userId)};
		if (!user)
			return;

		const Database::Track::pointer track {Database::Track::getById(session, listen.trackId)};
		if (!track)
			return;

		Database::TrackList::pointer tracklist {getListensTrackList(session, user)};
		if (!tracklist)
			tracklist = Database::TrackList::create(session, historyTracklistName, Database::TrackList::Type::Internal, false, user);

		Database::TrackListEntry::create(session, track, getListensTrackList(session, user), timePoint);
	}

} // Scrobbling

