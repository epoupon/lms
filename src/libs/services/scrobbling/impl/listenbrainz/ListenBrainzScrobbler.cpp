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
#include "services/database/StarredArtist.hpp"
#include "services/database/StarredRelease.hpp"
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

	template <typename StarredObjType>
	void onStarred(Database::Session& session, typename StarredObjType::IdType id)
	{
		auto transaction {session.createUniqueTransaction()};

		if (auto starredObj {StarredObjType::find(session, id)})
		{
			// maybe in the future this will be supported by ListenBrainz so set it to PendingAdd
			starredObj.modify()->setScrobblingState(Database::ScrobblingState::PendingAdd);
		}
	}

	template <typename StarredObjType>
	void onUnstarred(Database::Session& session, typename StarredObjType::IdType id)
	{
		auto transaction {session.createUniqueTransaction()};

		if (auto starredObj {StarredObjType::find(session, id)})
			starredObj.remove();
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
		, _feedbacksSynchronizer {_ioContext, db, *_client}
	{
		LOG(INFO) << "Starting ListenBrainz scrobbler... API endpoint = '" << _baseAPIUrl << "'";
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
	Scrobbler::onStarred(StarredArtistId starredArtistId)
	{
		::onStarred<StarredArtist>(_db.getTLSSession(), starredArtistId);
	}

	void
	Scrobbler::onUnstarred(StarredArtistId starredArtistId)
	{
		::onUnstarred<StarredArtist>(_db.getTLSSession(), starredArtistId);
	}

	void
	Scrobbler::onStarred(StarredReleaseId starredReleaseId)
	{
		::onStarred<StarredRelease>(_db.getTLSSession(), starredReleaseId);
	}

	void
	Scrobbler::onUnstarred(StarredReleaseId starredReleaseId)
	{
		::onUnstarred<StarredRelease>(_db.getTLSSession(), starredReleaseId);
	}

	void
	Scrobbler::onStarred(StarredTrackId starredTrackId)
	{
		_feedbacksSynchronizer.enqueFeedback(FeedbackType::Love, starredTrackId);
	}

	void
	Scrobbler::onUnstarred(StarredTrackId starredtrackId)
	{
		_feedbacksSynchronizer.enqueFeedback(FeedbackType::Erase, starredtrackId);
	}
} // namespace Scrobbling::ListenBrainz

