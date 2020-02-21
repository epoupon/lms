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
#include "database/TrackList.hpp"

#include <cassert>

#include "utils/Logger.hpp"

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/User.hpp"
#include "database/Track.hpp"

namespace Database {

TrackList::TrackList(const std::string& name, Type type, bool isPublic, Wt::Dbo::ptr<User> user)
: _name {name},
 _type {type},
 _isPublic {isPublic},
 _user {user}
{

}

TrackList::pointer
TrackList::create(Session& session, const std::string& name, Type type, bool isPublic, Wt::Dbo::ptr<User> user)
{
	session.checkUniqueLocked();
	assert(user);

	auto res = session.getDboSession().add( std::make_unique<TrackList>(name, type, isPublic, user) );
	session.getDboSession().flush();

	return res;
}

TrackList::pointer
TrackList::get(Session& session, const std::string& name, Type type, Wt::Dbo::ptr<User> user)
{
	session.checkSharedLocked();
	assert(user);

	return session.getDboSession().find<TrackList>()
		.where("name = ?").bind(name)
		.where("type = ?").bind(type)
		.where("user_id = ?").bind(user.id());
}

std::vector<TrackList::pointer>
TrackList::getAll(Session& session)
{
	session.checkSharedLocked();
	Wt::Dbo::collection<TrackList::pointer> res = session.getDboSession().find<TrackList>();

	return std::vector<TrackList::pointer>(res.begin(), res.end());
}

std::vector<TrackList::pointer>
TrackList::getAll(Session& session, Wt::Dbo::ptr<User> user)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<TrackList::pointer> res = session.getDboSession().find<TrackList>()
		.where("user_id = ?").bind(user.id())
		.orderBy("name COLLATE NOCASE");

	return std::vector<TrackList::pointer>(res.begin(), res.end());
}

std::vector<TrackList::pointer>
TrackList::getAll(Session& session, Wt::Dbo::ptr<User> user, Type type)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<TrackList::pointer> res = session.getDboSession().find<TrackList>()
		.where("user_id = ?").bind(user.id())
		.where("type = ?").bind(type)
		.orderBy("name COLLATE NOCASE");

	return std::vector<TrackList::pointer>(res.begin(), res.end());
}

TrackList::pointer
TrackList::getById(Session& session, IdType id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<TrackList>().where("id = ?").bind(id);
}


std::vector<Wt::Dbo::ptr<TrackListEntry>>
TrackList::getEntries(std::optional<std::size_t> offset, std::optional<std::size_t> size) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Wt::Dbo::ptr<TrackListEntry>> entries =
		session()->find<TrackListEntry>()
		.where("tracklist_id = ?").bind(self().id())
		.orderBy("id")
		.limit(size ? static_cast<int>(*size) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1);

	return std::vector<Wt::Dbo::ptr<TrackListEntry>>(entries.begin(), entries.end());
}

std::vector<Wt::Dbo::ptr<TrackListEntry>>
TrackList::getEntriesReverse(std::optional<std::size_t> offset, std::optional<std::size_t> size) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Wt::Dbo::ptr<TrackListEntry>> entries =
		session()->find<TrackListEntry>()
		.where("tracklist_id = ?").bind(self().id())
		.orderBy("id DESC")
		.limit(size ? static_cast<int>(*size) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1);

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

std::vector<Track::pointer>
TrackList::getSimilarTracks(std::optional<std::size_t> offset, std::optional<std::size_t> size) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::Query<Track::pointer> query {session()->query<Track::pointer>(
			"SELECT t FROM track t"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
				" WHERE "
					" (t_c.cluster_id IN (SELECT c.id from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id WHERE p.id = ?)"
					" AND t.id NOT IN (SELECT tracklist_t.id FROM track tracklist_t INNER JOIN tracklist_entry t_e ON t_e.track_id = tracklist_t.id WHERE t_e.tracklist_id = ?))"
				)
		.bind(self()->id())
		.bind(self()->id())
		.groupBy("t.id")
		.orderBy("COUNT(*) DESC")
		.limit(size ? static_cast<int>(*size) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1)};

	Wt::Dbo::collection<Track::pointer> tracks = query;
	return std::vector<Track::pointer>(tracks.begin(), tracks.end());
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

std::chrono::milliseconds
TrackList::getDuration() const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	using milli = std::chrono::duration<int, std::milli>;

	Wt::Dbo::Query<milli> query {session()->query<milli>("SELECT SUM(duration) FROM track t INNER JOIN tracklist_entry p_e ON t.id = p_e.track_id")
			.where("p_e.tracklist_id = ?").bind(self()->id())};

	return query.resultValue();
}

std::vector<Artist::pointer>
TrackList::getTopArtists(std::size_t limit) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Artist::pointer> res = session()->query<Artist::pointer>("SELECT a from artist a INNER JOIN track t ON t.id = t_a_l.track_id INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")
		.where("p.id = ?").bind(self()->id())
		.groupBy("a.id")
		.orderBy("COUNT(a.id) DESC")
		.limit(static_cast<int>(limit));

	return std::vector<Artist::pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
TrackList::getTopReleases(std::size_t limit) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Release::pointer> res = session()->query<Release::pointer>("SELECT r from release r INNER JOIN track t ON t.release_id = r.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")
		.where("p.id = ?").bind(self()->id())
		.groupBy("r.id")
		.orderBy("COUNT(r.id) DESC")
		.limit(static_cast<int>(limit));

	return std::vector<Release::pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
TrackList::getTopTracks(std::size_t limit) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Track::pointer> res = session()->query<Track::pointer>("SELECT t from track t INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")
		.where("p.id = ?").bind(self()->id())
		.groupBy("t.id")
		.orderBy("COUNT(t.id) DESC")
		.limit(static_cast<int>(limit));

	return std::vector<Track::pointer>(res.begin(), res.end());
}

TrackListEntry::TrackListEntry(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist)
: _track(track),
 _tracklist(tracklist)
{

}

TrackListEntry::pointer
TrackListEntry::create(Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist)
{
	session.checkUniqueLocked();
	assert(track);
	assert(tracklist);

	auto res = session.getDboSession().add( std::make_unique<TrackListEntry>( track, tracklist) );
	session.getDboSession().flush();

	return res;
}

TrackListEntry::pointer
TrackListEntry::getById(Session& session, IdType id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<TrackListEntry>().where("id = ?").bind(id);
}

} // namespace Database
