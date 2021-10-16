/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "lmscore/database/TrackBookmark.hpp"

#include "lmscore/database/Session.hpp"
#include "lmscore/database/Track.hpp"
#include "lmscore/database/User.hpp"
#include "Traits.hpp"

namespace Database {

TrackBookmark::TrackBookmark(ObjectPtr<User> user, ObjectPtr<Track> track)
: _user {getDboPtr(user)},
_track {getDboPtr(track)}
{
}

TrackBookmark::pointer
TrackBookmark::create(Session& session, ObjectPtr<User> user, ObjectPtr<Track> track)
{
	session.checkUniqueLocked();

	TrackBookmark::pointer res {session.getDboSession().add(std::make_unique<TrackBookmark>(user, track))};
	session.getDboSession().flush();

	return res;
}

std::vector<TrackBookmark::pointer>
TrackBookmark::getAll(Session& session)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<TrackBookmark>().resultList()};
	return std::vector<TrackBookmark::pointer>(std::cbegin(res), std::cend(res));
}

std::vector<TrackBookmark::pointer>
TrackBookmark::getByUser(Session& session, User::pointer user)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<TrackBookmark>()
				.where("user_id = ?").bind(user->getId())
				.resultList()};

	return std::vector<TrackBookmark::pointer>(std::cbegin(res), std::cend(res));
}

TrackBookmark::pointer
TrackBookmark::getByUser(Session& session, ObjectPtr<User> user, ObjectPtr<Track> track)
{
	session.checkSharedLocked();

	return session.getDboSession().find<TrackBookmark>()
		.where("user_id = ?").bind(user->getId())
		.where("track_id = ?").bind(track->getId())
		.resultValue();
}

TrackBookmark::pointer
TrackBookmark::getById(Session& session, TrackBookmarkId id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<TrackBookmark>()
		.where("id = ?").bind(id)
		.resultValue();
}


} // namespace Database

