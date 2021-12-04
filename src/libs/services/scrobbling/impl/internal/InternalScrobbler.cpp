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

#include "services/database/Db.hpp"
#include "services/database/Listen.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "services/database/Track.hpp"

namespace Scrobbling
{
	InternalScrobbler::InternalScrobbler(Database::Db& db)
		: _db {db}
	{}

	void
	InternalScrobbler::listenStarted(const Listen&)
	{
		// nothing to do
	}

	void
	InternalScrobbler::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
	{
		// only record tracks that have been played for at least of few seconds...
		if (duration && *duration < std::chrono::seconds {5})
			return;

		addTimedListen({listen, Wt::WDateTime::currentDateTime()});
	}

	void
	InternalScrobbler::addTimedListen(const TimedListen& listen)
	{
		Database::Session& session {_db.getTLSSession()};
		auto transaction {session.createUniqueTransaction()};

		if (Database::Listen::find(session, listen.userId, listen.trackId, Database::Scrobbler::Internal, listen.listenedAt))
			return;

		const Database::User::pointer user {Database::User::find(session, listen.userId)};
		if (!user)
			return;

		const Database::Track::pointer track {Database::Track::find(session, listen.trackId)};
		if (!track)
			return;

		Database::Listen::create(session, user, track, Database::Scrobbler::Internal, listen.listenedAt);
	}
} // Scrobbling

