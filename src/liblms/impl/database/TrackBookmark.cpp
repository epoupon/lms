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

#include "database/TrackBookmark.hpp"

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"

namespace Database {

TrackBookmark::TrackBookmark(Wt::Dbo::ptr<User> user, Wt::Dbo::ptr<Track> track)
: _user {user},
_track {track}
{
}


TrackBookmark::pointer
TrackBookmark::create(Session& session, Wt::Dbo::ptr<User> user, Wt::Dbo::ptr<Track> track)
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

	Wt::Dbo::collection<TrackBookmark::pointer> res {session.getDboSession().find<TrackBookmark>()};

	return std::vector<TrackBookmark::pointer>(std::cbegin(res), std::cend(res));
}

std::vector<TrackBookmark::pointer>
TrackBookmark::getByUser(Session& session, Wt::Dbo::ptr<User> user)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<TrackBookmark::pointer> res
	{
		session.getDboSession().find<TrackBookmark>()
		.where("user_id = ?").bind(user.id())
	};

	return std::vector<TrackBookmark::pointer>(std::cbegin(res), std::cend(res));
}

TrackBookmark::pointer
TrackBookmark::getByUser(Session& session, Wt::Dbo::ptr<User> user, Wt::Dbo::ptr<Track> track)
{
	session.checkSharedLocked();

	return session.getDboSession().find<TrackBookmark>()
		.where("user_id = ?").bind(user.id())
		.where("track_id = ?").bind(track.id());
}

TrackBookmark::pointer
TrackBookmark::getById(Session& session, IdType id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<TrackBookmark>()
		.where("id = ?").bind(id);
}


} // namespace Database

