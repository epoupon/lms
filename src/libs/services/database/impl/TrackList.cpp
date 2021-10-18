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
#include "StringViewTraits.hpp"
#include "Traits.hpp"

namespace Database {

TrackList::TrackList(std::string_view name, Type type, bool isPublic, ObjectPtr<User> user)
: _name {name},
 _type {type},
 _isPublic {isPublic},
 _user {getDboPtr(user)}
{

}

TrackList::pointer
TrackList::create(Session& session, std::string_view name, Type type, bool isPublic, ObjectPtr<User> user)
{
	session.checkUniqueLocked();
	assert(user);

	TrackList::pointer res {session.getDboSession().add( std::make_unique<TrackList>(name, type, isPublic, user) )};
	session.getDboSession().flush();

	return res;
}

TrackList::pointer
TrackList::get(Session& session, std::string_view name, Type type, ObjectPtr<User> user)
{
	session.checkSharedLocked();
	assert(user);

	return session.getDboSession().find<TrackList>()
		.where("name = ?").bind(name)
		.where("type = ?").bind(type)
		.where("user_id = ?").bind(user->getId()).resultValue();
}

std::vector<TrackList::pointer>
TrackList::getAll(Session& session)
{
	session.checkSharedLocked();

	auto res = session.getDboSession().find<TrackList>().resultList();
	return std::vector<TrackList::pointer>(res.begin(), res.end());
}

std::vector<TrackList::pointer>
TrackList::getAll(Session& session, ObjectPtr<User> user)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<TrackList>()
		.where("user_id = ?").bind(user->getId())
		.orderBy("name COLLATE NOCASE")
		.resultList()};

	return std::vector<TrackList::pointer>(res.begin(), res.end());
}

std::vector<TrackList::pointer>
TrackList::getAll(Session& session, ObjectPtr<User> user, Type type)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<TrackList>()
		.where("user_id = ?").bind(user->getId())
		.where("type = ?").bind(type)
		.orderBy("name COLLATE NOCASE")
		.resultList()};

	return std::vector<TrackList::pointer>(res.begin(), res.end());
}

TrackList::pointer
TrackList::getById(Session& session, TrackListId id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<TrackList>().where("id = ?").bind(id).resultValue();
}

bool
TrackList::isEmpty() const
{
	return _entries.empty();
}

std::size_t
TrackList::getCount() const
{
	return _entries.size();
}

TrackListEntry::pointer
TrackList::getEntry(std::size_t pos) const
{
	TrackListEntry::pointer res;

	auto entries = getEntries(pos, 1);
	if (!entries.empty())
		res = entries.front();

	return res;
}

std::vector<TrackListEntry::pointer>
TrackList::getEntries(std::optional<std::size_t> offset, std::optional<std::size_t> size) const
{
	assert(session());

	auto entries {
		session()->find<TrackListEntry>()
		.where("tracklist_id = ?").bind(getId())
		.orderBy("id")
		.limit(size ? static_cast<int>(*size) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1)
		.resultList()};

	return std::vector<TrackListEntry::pointer>(entries.begin(), entries.end());
}

TrackListEntry::pointer
TrackList::getEntryByTrackAndDateTime(ObjectPtr<Track> track, const Wt::WDateTime& dateTime) const
{
	assert(session());

	return session()->find<TrackListEntry>()
			.where("tracklist_id = ?").bind(getId())
			.where("track_id = ?").bind(track->getId())
			.where("date_time = ?").bind(Wt::WDateTime::fromTime_t(dateTime.toTime_t()))
			.resultValue();
}

static
Wt::Dbo::Query<Wt::Dbo::ptr<Artist>>
createArtistsQuery(Wt::Dbo::Session& session, const std::string& queryStr, TrackListId tracklistId, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType)
{
	auto query {session.query<Wt::Dbo::ptr<Artist>>(queryStr)};
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
Wt::Dbo::Query<Wt::Dbo::ptr<Release>>
createReleasesQuery(Wt::Dbo::Session& session, const std::string& queryStr, TrackListId tracklistId, const std::vector<ClusterId>& clusterIds)
{
	auto query {session.query<Wt::Dbo::ptr<Release>>(queryStr)};
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
		for (ClusterId id : clusterIds)
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
Wt::Dbo::Query<Wt::Dbo::ptr<Track>>
createTracksQuery(Wt::Dbo::Session& session, TrackListId tracklistId, const std::vector<ClusterId>& clusterIds)
{
	auto query {session.query<Wt::Dbo::ptr<Track>>("SELECT t from track t INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")};

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
			clusterClause.Or(WhereClause("c.id = ?")).bind(id.toString());
			query.bind(id);
		}

		oss << " " << clusterClause.get();
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size() << ")";

		query.where(oss.str());
	}

	return query;
}

std::vector<Artist::pointer>
TrackList::getArtistsReverse(const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, std::optional<Range> range, bool& moreResults) const
{
	assert(session());

	auto collection {createArtistsQuery(*session(), "SELECT a from artist a", getId(), clusterIds, linkType)
		.groupBy("a.id").having("p_e.date_time = MAX(p_e.date_time)")
		.orderBy("p_e.date_time DESC")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1)
		.resultList()};

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
TrackList::getReleasesReverse(const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults) const
{
	assert(session());

	auto collection {createReleasesQuery(*session(), "SELECT r from release r", getId(), clusterIds)
		.groupBy("r.id").having("p_e.date_time = MAX(p_e.date_time)")
		.orderBy("p_e.date_time DESC")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1)
		.resultList()};

	std::vector<Release::pointer> res(collection.begin(), collection.end());
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
TrackList::getTracksReverse(const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults) const
{
	assert(session());

	auto collection {createTracksQuery(*session(), getId(), clusterIds)
		.groupBy("t.id").having("p_e.date_time = MAX(p_e.date_time)")
		.orderBy("p_e.date_time DESC")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1)
		.resultList()};

	std::vector<Track::pointer> res(collection.begin(), collection.end());
	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Cluster::pointer>
TrackList::getClusters() const
{
	assert(session());

	auto res {session()->query<Wt::Dbo::ptr<Cluster>>("SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id")
		.where("p.id = ?").bind(getId())
		.groupBy("c.id")
		.orderBy("COUNT(c.id) DESC")
		.resultList()};

	return std::vector<Cluster::pointer>(res.begin(), res.end());
}

bool
TrackList::hasTrack(TrackId trackId) const
{
	assert(session());

	Wt::Dbo::collection<TrackListEntry::pointer> res = session()->query<TrackListEntry::pointer>("SELECT p_e from tracklist_entry p_e INNER JOIN tracklist p ON p_e.tracklist_id = p.id")
		.where("p_e.track_id = ?").bind(trackId)
		.where("p.id = ?").bind(getId());

	return res.size() > 0;
}

std::vector<Track::pointer>
TrackList::getSimilarTracks(std::optional<std::size_t> offset, std::optional<std::size_t> size) const
{
	assert(session());

	auto res {session()->query<Wt::Dbo::ptr<Track>>(
			"SELECT t FROM track t"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
				" WHERE "
					" (t_c.cluster_id IN (SELECT c.id from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id WHERE p.id = ?)"
					" AND t.id NOT IN (SELECT tracklist_t.id FROM track tracklist_t INNER JOIN tracklist_entry t_e ON t_e.track_id = tracklist_t.id WHERE t_e.tracklist_id = ?))"
				)
		.bind(getId())
		.bind(getId())
		.groupBy("t.id")
		.orderBy("COUNT(*) DESC, RANDOM()")
		.limit(size ? static_cast<int>(*size) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1)
		.resultList()};

	return std::vector<Track::pointer>(res.begin(), res.end());
}

std::vector<TrackId>
TrackList::getTrackIds() const
{
	assert(session());

	Wt::Dbo::collection<TrackId> res = session()->query<TrackId>("SELECT p_e.track_id from tracklist_entry p_e INNER JOIN tracklist p ON p_e.tracklist_id = p.id")
		.where("p.id = ?").bind(getId());

	return std::vector<TrackId>(res.begin(), res.end());
}

std::chrono::milliseconds
TrackList::getDuration() const
{
	assert(session());

	using milli = std::chrono::duration<int, std::milli>;

	Wt::Dbo::Query<milli> query {session()->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t INNER JOIN tracklist_entry p_e ON t.id = p_e.track_id")
			.where("p_e.tracklist_id = ?").bind(getId())};

	return query.resultValue();
}

std::vector<Artist::pointer>
TrackList::getTopArtists(const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, std::optional<Range> range, bool& moreResults) const
{
	assert(session());

	auto query {createArtistsQuery(*session(), "SELECT a from artist a", getId(), clusterIds, linkType)};

	auto collection {query
		.orderBy("COUNT(a.id) DESC")
		.groupBy("a.id")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1)
		.resultList()};

	std::vector<Artist::pointer> res(collection.begin(), collection.end());

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
TrackList::getTopReleases(const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults) const
{
	assert(session());

	auto query {createReleasesQuery(*session(), "SELECT r from release r", getId(), clusterIds)};
	auto collection {query
		.orderBy("COUNT(r.id) DESC")
		.groupBy("r.id")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1)
		.resultList()};

	std::vector<Release::pointer> res(collection.begin(), collection.end());
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
TrackList::getTopTracks(const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults) const
{
	assert(session());

	auto query {createTracksQuery(*session(), getId(), clusterIds)};
	auto collection {query
		.orderBy("COUNT(t.id) DESC")
		.groupBy("t.id")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1)
		.resultList()};

	std::vector<Track::pointer> res(collection.begin(), collection.end());
	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

TrackListEntry::TrackListEntry(ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime)
: _dateTime {Wt::WDateTime::fromTime_t(dateTime.toTime_t())} // force second resolution
, _track {getDboPtr(track)}
, _tracklist {getDboPtr(tracklist)}
{
	assert(_dateTime.isValid());
}

TrackListEntry::pointer
TrackListEntry::create(Session& session, ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime)
{
	session.checkUniqueLocked();
	assert(track);
	assert(tracklist);

	auto res = session.getDboSession().add(std::make_unique<TrackListEntry>( track, tracklist, dateTime));
	session.getDboSession().flush();

	return res;
}

TrackListEntry::pointer
TrackListEntry::getById(Session& session, TrackListEntryId id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<TrackListEntry>().where("id = ?").bind(id).resultValue();
}

} // namespace Database
