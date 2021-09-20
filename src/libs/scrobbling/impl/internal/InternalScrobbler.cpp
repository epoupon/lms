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

#include "InternalScrobbler.hpp"

#include "database/Db.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"
#include "utils/Logger.hpp"

namespace Scrobbling
{
	static const std::string historyTracklistName {"__scrobbler_internal_history__"};

	InternalScrobbler::InternalScrobbler(Database::Db& db)
	: _db {db}
	{}

	void
	InternalScrobbler::listenStarted(const Listen& /*listen*/)
	{
		// nothing to do
	}

	void
	InternalScrobbler::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
	{
		// record tracks that have been played for at least of few seconds...
		if (duration && *duration < std::chrono::seconds {5})
			return;

		addTimedListen({listen, Wt::WDateTime::currentDateTime()});
	}

	void
	InternalScrobbler::addTimedListen(const TimedListen& listen)
	{
		Database::Session& session {_db.getTLSSession()};

		auto transaction {session.createUniqueTransaction()};

		const Database::User::pointer user {Database::User::getById(session, listen.userId)};
		if (!user)
			return;

		Database::TrackList::pointer tracklist {getListensTrackList(session, user)};
		if (!tracklist)
			tracklist = Database::TrackList::create(session, historyTracklistName, Database::TrackList::Type::Internal, false, user);

		const Database::Track::pointer track {Database::Track::getById(session, listen.trackId)};
		if (!track)
			return;

		Database::TrackListEntry::create(session, track, getListensTrackList(session, user), listen.listenedAt);
	}

	Database::TrackList::pointer
	InternalScrobbler::getListensTrackList(Database::Session& session, Database::ObjectPtr<Database::User> user)
	{
		return Database::TrackList::get(session, historyTracklistName, Database::TrackList::Type::Internal, user);
	}

} // Scrobbling

