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
#include "SqlQuery.hpp"

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

std::size_t
TrackList::getCount() const
{
	return _entries.size();
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

static
Wt::Dbo::Query<Artist::pointer>
createArtistsQuery(Wt::Dbo::Session& session, const std::string& queryStr, IdType tracklistId, const std::set<IdType>& clusterIds, std::optional<TrackArtistLink::Type> linkType)
{
	auto query {session.query<Artist::pointer>(queryStr)};
	query.join("track t ON t.id = t_a_l.track_id");
	query.join("track_artist_link t_a_l ON t_a_l.artist_id = a.id");
	query.join("tracklist_entry p_e ON p_e.track_id = t.id");
	query.join("tracklist p ON p.id = p_e.tracklist_id");

	query.where("p.id = ?").bind(tracklistId);

	if (linkType)
		query.where("t_a_l.type = ?").bind(*linkType);

	if (!clusterIds.empty())
	{
		std::ostringstream oss;
		oss << "a.id IN (SELECT DISTINCT a.id FROM artist a"
				" INNER JOIN track t ON t.id = t_a_l.track_id"
				" INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
				" INNER JOIN cluster c ON c.id = t_c.cluster_id"
				" INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;
		for (auto id : clusterIds)
		{
			clusterClause.Or(WhereClause("c.id = ?"));
			query.bind(id);
		}

		oss << " " << clusterClause.get();
		oss << " GROUP BY t.id,a.id HAVING COUNT(DISTINCT c.id) = " << clusterIds.size() << ")";

		query.where(oss.str());
	}

	return query;
}

static
Wt::Dbo::Query<Release::pointer>
createReleasesQuery(Wt::Dbo::Session& session, const std::string& queryStr, IdType tracklistId, const std::set<IdType>& clusterIds)
{
	auto query {session.query<Release::pointer>(queryStr)};
	query.join("track t ON t.release_id = r.id");
	query.join("tracklist_entry p_e ON p_e.track_id = t.id");
	query.join("tracklist p ON p.id = p_e.tracklist_id");

	query.where("p.id = ?").bind(tracklistId);

	if (!clusterIds.empty())
	{
		std::ostringstream oss;
		oss << "r.id IN (SELECT DISTINCT r.id FROM release r"
				" INNER JOIN track t ON t.release_id = r.id"
				" INNER JOIN cluster c ON c.id = t_c.cluster_id"
				" INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;
		for (auto id : clusterIds)
		{
			clusterClause.Or(WhereClause("c.id = ?"));
			query.bind(id);
		}

		oss << " " << clusterClause.get();
		oss << " GROUP BY t.id HAVING COUNT(DISTINCT c.id) = " << clusterIds.size() << ")";

		query.where(oss.str());
	}

	return query;
}

static
Wt::Dbo::Query<Track::pointer>
createTracksQuery(Wt::Dbo::Session& session, IdType tracklistId, const std::set<IdType>& clusterIds)
{
	auto query {session.query<Track::pointer>("SELECT t from track t INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")};

	query.where("p.id = ?").bind(tracklistId);

	if (!clusterIds.empty())
	{
		std::ostringstream oss;
		oss << "t.id IN (SELECT DISTINCT t.id FROM track t"
				" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
				" INNER JOIN cluster c ON c.id = t_c.cluster_id";

		WhereClause clusterClause;
		for (auto id : clusterIds)
		{
			clusterClause.Or(WhereClause("c.id = ?")).bind(std::to_string(id));
			query.bind(id);
		}

		oss << " " << clusterClause.get();
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size() << ")";

		query.where(oss.str());
	}

	return query;
}

std::vector<Artist::pointer>
TrackList::getArtistsReverse(const std::set<IdType>& clusterIds, std::optional<TrackArtistLink::Type> linkType, std::optional<Range> range, bool& moreResults) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Artist::pointer> collection = createArtistsQuery(*session(), "SELECT DISTINCT a from artist a", self()->id(), clusterIds, linkType)
		.orderBy("p_e.id DESC")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1);

	auto res {std::vector<Artist::pointer>(collection.begin(), collection.end())};
	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Release::pointer>
TrackList::getReleasesReverse(const std::set<IdType>& clusterIds, std::optional<Range> range, bool& moreResults) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Release::pointer> collection = createReleasesQuery(*session(), "SELECT DISTINCT r from release r", self()->id(), clusterIds)
		.orderBy("p_e.id DESC")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1);

	auto res {std::vector<Release::pointer>(collection.begin(), collection.end())};
	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Track::pointer>
TrackList::getTracksReverse(const std::set<IdType>& clusterIds, std::optional<Range> range, bool& moreResults) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Track::pointer> collection = createTracksQuery(*session(), self()->id(), clusterIds)
		.orderBy("p_e.id DESC")
		.groupBy("t.id")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1);

	auto res {std::vector<Track::pointer>(collection.begin(), collection.end())};
	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
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
TrackList::getTopArtists(const std::set<IdType>& clusterIds, std::optional<TrackArtistLink::Type> linkType, std::optional<Range> range, bool& moreResults) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	auto query {createArtistsQuery(*session(), "SELECT a from artist a", self()->id(), clusterIds, linkType)};

	Wt::Dbo::collection<Artist::pointer> collection = query
		.orderBy("COUNT(a.id) DESC")
		.groupBy("a.id")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1);

	auto res {std::vector<Artist::pointer>(collection.begin(), collection.end())};

	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;


	return res;
}

std::vector<Release::pointer>
TrackList::getTopReleases(const std::set<IdType>& clusterIds, std::optional<Range> range, bool& moreResults) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	auto query {createReleasesQuery(*session(), "SELECT r from release r", self()->id(), clusterIds)};

	Wt::Dbo::collection<Release::pointer> collection = query
		.orderBy("COUNT(r.id) DESC")
		.groupBy("r.id")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1);

	auto res {std::vector<Release::pointer>(collection.begin(), collection.end())};

	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Track::pointer>
TrackList::getTopTracks(const std::set<IdType>& clusterIds, std::optional<Range> range, bool& moreResults) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	auto query {createTracksQuery(*session(), self()->id(), clusterIds)};

	Wt::Dbo::collection<Track::pointer> collection = query
		.orderBy("COUNT(t.id) DESC")
		.groupBy("t.id")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1);

	auto res {std::vector<Track::pointer>(collection.begin(), collection.end())};

	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
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
