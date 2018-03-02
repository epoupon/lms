/*
 * Copyright (C) 2014 Emeric Poupon
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

#include "Types.hpp"

namespace Database {

Playlist::Playlist()
: _isPublic(false)
{

}

Playlist::Playlist(std::string name, bool isPublic, Wt::Dbo::ptr<User> user)
: _name(name),
 _isPublic(isPublic),
 _user(user)
{

}

Playlist::pointer
Playlist::create(Wt::Dbo::Session& session, std::string name, bool isPublic, Wt::Dbo::ptr<User> user)
{
	return session.add( new Playlist(name, isPublic, user) );
}

PlaylistEntry::PlaylistEntry()
: _pos(0)
{

}

Playlist::pointer
Playlist::get(Wt::Dbo::Session& session, std::string name, Wt::Dbo::ptr<User> user)
{
	return session.find<Playlist>().where("name = ? AND user_id = ?").bind(name).bind(user.id());
}

std::vector<Playlist::pointer>
Playlist::getAll(Wt::Dbo::Session& session, Wt::Dbo::ptr<User> user)
{
	Wt::Dbo::collection<Playlist::pointer> res = session.find<Playlist>().where("user_id = ?").bind(user.id()).orderBy("name");

	return std::vector<Playlist::pointer>(res.begin(), res.end());
}

PlaylistEntry::PlaylistEntry(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Playlist> playlist, int pos)
: _pos(pos),
 _track(track),
 _playlist(playlist)
{

}

PlaylistEntry::pointer
PlaylistEntry::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Playlist> playlist, int pos)
{
	return session.add( new PlaylistEntry( track, playlist, pos) );
}


std::vector<Wt::Dbo::ptr<Track>>
Playlist::getTracks(int offset, int size, bool& moreResults) const
{
	assert(session());

	moreResults = false;

	Wt::Dbo::collection<Wt::Dbo::ptr<PlaylistEntry>> entries
		= session()->find<PlaylistEntry>()
		.where("playlist_id = ?").bind(self().id())
		.orderBy("pos")
		.limit(size != -1 ? size + 1 : -1)
		.offset(offset);

	std::vector<Wt::Dbo::ptr<Track>> res;

	for (auto entry : entries)
	{
		if (size != -1 && res.size() == static_cast<std::size_t>(size))
		{
			moreResults = true;
			break;
		}

		res.push_back(entry->getTrack());
	}

	return res;
}

std::size_t
Playlist::getCount() const
{
	return _entries.size();
}

} // namespace Database
