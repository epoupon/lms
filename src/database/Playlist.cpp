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
#include "Playlist.hpp"

#include <cassert>
#include <random>

#include "Cluster.hpp"
#include "User.hpp"
#include "Track.hpp"

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
	return session.add( std::make_unique<Playlist>(name, isPublic, user) );
}

PlaylistEntry::PlaylistEntry()
{
}

PlaylistEntry::pointer
PlaylistEntry::getById(Wt::Dbo::Session& session, IdType id)
{
	return session.find<PlaylistEntry>().where("id = ?").bind(id);
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

PlaylistEntry::PlaylistEntry(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Playlist> playlist)
: _track(track),
 _playlist(playlist)
{

}

PlaylistEntry::pointer
PlaylistEntry::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Playlist> playlist)
{
	return session.add( std::make_unique<PlaylistEntry>( track, playlist) );
}


std::vector<Wt::Dbo::ptr<PlaylistEntry>>
Playlist::getEntries(int offset, int size, bool& moreResults) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	moreResults = false;

	Wt::Dbo::collection<Wt::Dbo::ptr<PlaylistEntry>> entries =
		session()->find<PlaylistEntry>()
		.where("playlist_id = ?").bind(self().id())
		.orderBy("id")
		.limit(size != -1 ? size + 1 : -1)
		.offset(offset);

	std::vector<Wt::Dbo::ptr<PlaylistEntry>> res;

	for (auto entry : entries)
	{
		if (size != -1 && res.size() == static_cast<std::size_t>(size))
		{
			moreResults = true;
			break;
		}

		res.push_back(entry);
	}

	return res;
}

std::vector<Wt::Dbo::ptr<PlaylistEntry>>
Playlist::getAllEntries() const
{
	bool moreResults;
	return getEntries(-1, -1, moreResults);
}

Wt::Dbo::ptr<PlaylistEntry>
Playlist::getEntry(std::size_t pos) const
{
	Wt::Dbo::ptr<PlaylistEntry> res;

	bool moreResults;
	auto entries = getEntries(pos, 1, moreResults);
	if (!entries.empty())
		res = entries.front();

	return res;
}

std::size_t
Playlist::getCount() const
{
	return _entries.size();
}

std::vector<Wt::Dbo::ptr<Cluster>>
Playlist::getClusters() const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Cluster::pointer> res = session()->query<Cluster::pointer>("SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN playlist_entry p_e ON p_e.track_id = t.id INNER JOIN playlist p ON p.id = p_e.playlist_id")
		.where("p.id = ?").bind(self()->id())
		.groupBy("c.id")
		.orderBy("COUNT(c.id) DESC");

	return std::vector<Wt::Dbo::ptr<Cluster>>(res.begin(), res.end());
}

bool
Playlist::hasTrack(IdType trackId) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<PlaylistEntry::pointer> res = session()->query<PlaylistEntry::pointer>("SELECT p_e from playlist_entry p_e INNER JOIN playlist p ON p_e.playlist_id = p.id")
		.where("p_e.track_id = ?").bind(trackId)
		.where("p.id = ?").bind(self()->id());

	return res.size() > 0;
}

std::vector<IdType>
Playlist::getTrackIds() const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<IdType> res = session()->query<IdType>("SELECT p_e.track_id from playlist_entry p_e INNER JOIN playlist p ON p_e.playlist_id = p.id")
		.where("p.id = ?").bind(self()->id());

	return std::vector<IdType>(res.begin(), res.end());
}

void
Playlist::shuffle()
{
	assert(session());

	auto entries = getAllEntries();

	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	std::shuffle(entries.begin(), entries.end(), randGenerator);

	clear();
	for (auto entry : entries)
		PlaylistEntry::create(*session(), entry->getTrack(), self());
}

} // namespace Database
