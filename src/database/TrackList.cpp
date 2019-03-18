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
#include "TrackList.hpp"

#include <cassert>
#include <random>

#include "utils/Logger.hpp"

#include "Artist.hpp"
#include "Cluster.hpp"
#include "Release.hpp"
#include "User.hpp"
#include "Track.hpp"

namespace Database {

TrackList::TrackList()
: _isPublic(false)
{

}

TrackList::TrackList(std::string name, bool isPublic, Wt::Dbo::ptr<User> user)
: _name(name),
 _isPublic(isPublic),
 _user(user)
{

}

TrackList::pointer
TrackList::create(Wt::Dbo::Session& session, std::string name, bool isPublic, Wt::Dbo::ptr<User> user)
{
	assert(user);

	auto res = session.add( std::make_unique<TrackList>(name, isPublic, user) );
	session.flush();

	return res;
}

TrackListEntry::pointer
TrackList::add(IdType trackId)
{
	assert(session());
	assert(self());

	return TrackListEntry::create(*session(), Database::Track::getById(*session(), trackId), self());
}

TrackList::pointer
TrackList::get(Wt::Dbo::Session& session, std::string name, Wt::Dbo::ptr<User> user)
{
	return session.find<TrackList>().where("name = ? AND user_id = ?").bind(name).bind(user.id());
}

std::vector<TrackList::pointer>
TrackList::getAll(Wt::Dbo::Session& session, Wt::Dbo::ptr<User> user)
{
	Wt::Dbo::collection<TrackList::pointer> res = session.find<TrackList>().where("user_id = ?").bind(user.id()).orderBy("name");

	return std::vector<TrackList::pointer>(res.begin(), res.end());
}

TrackList::pointer
TrackList::getById(Wt::Dbo::Session& session, IdType id)
{
	return session.find<TrackList>().where("id = ?").bind(id);
}


std::vector<Wt::Dbo::ptr<TrackListEntry>>
TrackList::getEntries(int offset, int size) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Wt::Dbo::ptr<TrackListEntry>> entries =
		session()->find<TrackListEntry>()
		.where("tracklist_id = ?").bind(self().id())
		.orderBy("id")
		.limit(size)
		.offset(offset);

	return std::vector<Wt::Dbo::ptr<TrackListEntry>>(entries.begin(), entries.end());
}

std::vector<Wt::Dbo::ptr<TrackListEntry>>
TrackList::getEntriesReverse(int offset, int size) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Wt::Dbo::ptr<TrackListEntry>> entries =
		session()->find<TrackListEntry>()
		.where("tracklist_id = ?").bind(self().id())
		.orderBy("id DESC")
		.limit(size)
		.offset(offset);

	return std::vector<Wt::Dbo::ptr<TrackListEntry>>(entries.begin(), entries.end());
}

Wt::Dbo::ptr<TrackListEntry>
TrackList::getEntry(std::size_t pos) const
{
	Wt::Dbo::ptr<TrackListEntry> res;

	auto entries = getEntries(pos, 1);
	if (!entries.empty())
		res = entries.front();

	return res;
}

std::size_t
TrackList::getCount() const
{
	return _entries.size();
}

std::vector<Wt::Dbo::ptr<Cluster>>
TrackList::getClusters() const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Cluster::pointer> res = session()->query<Cluster::pointer>("SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")
		.where("p.id = ?").bind(self()->id())
		.groupBy("c.id")
		.orderBy("COUNT(c.id) DESC");

	return std::vector<Wt::Dbo::ptr<Cluster>>(res.begin(), res.end());
}

bool
TrackList::hasTrack(IdType trackId) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<TrackListEntry::pointer> res = session()->query<TrackListEntry::pointer>("SELECT p_e from tracklist_entry p_e INNER JOIN tracklist p ON p_e.tracklist_id = p.id")
		.where("p_e.track_id = ?").bind(trackId)
		.where("p.id = ?").bind(self()->id());

	return res.size() > 0;
}

std::vector<IdType>
TrackList::getTrackIds() const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<IdType> res = session()->query<IdType>("SELECT p_e.track_id from tracklist_entry p_e INNER JOIN tracklist p ON p_e.tracklist_id = p.id")
		.where("p.id = ?").bind(self()->id());

	return std::vector<IdType>(res.begin(), res.end());
}

void
TrackList::shuffle()
{
	assert(session());

	auto entries = getEntries();

	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	std::shuffle(entries.begin(), entries.end(), randGenerator);

	clear();
	for (auto entry : entries)
		TrackListEntry::create(*session(), entry->getTrack(), self());
}

std::vector<Artist::pointer>
TrackList::getTopArtists(int limit) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Artist::pointer> res = session()->query<Artist::pointer>("SELECT a from artist a INNER JOIN track t ON t.id = t_a.track_id INNER JOIN track_artist t_a ON t_a.artist_id = a.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")
		.where("p.id = ?").bind(self()->id())
		.groupBy("a.id")
		.orderBy("COUNT(a.id) DESC")
		.limit(limit);

	return std::vector<Artist::pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
TrackList::getTopReleases(int limit) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Release::pointer> res = session()->query<Release::pointer>("SELECT r from release r INNER JOIN track t ON t.release_id = r.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")
		.where("p.id = ?").bind(self()->id())
		.groupBy("r.id")
		.orderBy("COUNT(r.id) DESC")
		.limit(limit);

	return std::vector<Release::pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
TrackList::getTopTracks(int limit) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Track::pointer> res = session()->query<Track::pointer>("SELECT t from track t INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")
		.where("p.id = ?").bind(self()->id())
		.groupBy("t.id")
		.orderBy("COUNT(t.id) DESC")
		.limit(limit);

	return std::vector<Track::pointer>(res.begin(), res.end());
}

TrackListEntry::TrackListEntry(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist)
: _track(track),
 _tracklist(tracklist)
{

}

TrackListEntry::TrackListEntry()
{
}

TrackListEntry::pointer
TrackListEntry::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist)
{
	assert(track);
	assert(tracklist);

	auto res = session.add( std::make_unique<TrackListEntry>( track, tracklist) );
	session.flush();

	return res;
}

TrackListEntry::pointer
TrackListEntry::getById(Wt::Dbo::Session& session, IdType id)
{
	return session.find<TrackListEntry>().where("id = ?").bind(id);
}

} // namespace Database
