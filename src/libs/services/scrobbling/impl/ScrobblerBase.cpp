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

#pragma once

#include <string_view>
#include "services/scrobbling/Listen.hpp"

namespace Scrobbling
{
	ScrobblerBase::ScrobblerBase(Database::Db& db)
		: _db {db}
	{
	}

	Database::TrackListId
	ScrobberBase::getListensTrackList(Database::UserId userId, std::string_view trackListName)
	{
		std::string_view listensTrackListName {getListensTrackListName()};
		Database::Session& session {_db.getTLSSession()};

		{
			auto transaction {session.createSharedTransaction()};

			if (auto trackList {Database::TrackList::get(session, listensTrackListName, Database::TrackList::Type::Internal, userId)})
				return trackList->getId();
		}

		{
			auto transaction {session.createUniqueTransaction()};
			if (auto trackList {Database::TrackList::get(session, listensTrackListName, Database::TrackList::Type::Internal, userId)})
				return trackList->getId();

			const Database::User::pointer user {Database::User::getById(session, userId)};
			if (!user)
				return {};

			Database::TrackList::pointer trackList {Database::TrackList::create(session, listensTrackListName, Database::TrackList::Type::Internal, false, user)};
			return trackList->getId();
		}
	}

	bool
	ScrobberBase::saveTimedListen(const TimedListen& listen, std::string_view trackListName)
	{
		const Database::TrackListId trackListId {getListensTrackList(listen.userId)};
		if (!trackListId)
			return false;

		{

			return true;
		}
	}
} // ns Scrobbling

