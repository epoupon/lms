/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "database/Release.hpp"

#include "utils/Logger.hpp"

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"
#include "SqlQuery.hpp"

namespace Database
{

template <typename T>
static
Wt::Dbo::Query<T>
createQuery(Session& session,
			const std::string& queryStr,
			const std::set<IdType>& clusterIds,
			const std::vector<std::string>& keywords)
{

	auto query {session.getDboSession().query<T>(queryStr)};
	query.join("track t ON t.release_id = r.id");

	for (const std::string& keyword : keywords)
		query.where("r.name LIKE ?").bind("%%" + keyword + "%%");

	if (!clusterIds.empty())
	{
		std::ostringstream oss;
		oss << "r.id IN (SELECT DISTINCT r.id FROM release r"
			" INNER JOIN track t ON t.release_id = r.id"
			" INNER JOIN cluster c ON c.id = t_c.cluster_id"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;
		for (const IdType clusterId : clusterIds)
		{
			clusterClause.Or(WhereClause("c.id = ?"));
			query.bind(clusterId);
		}

		oss << " " << clusterClause.get();
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size() << ")";

		query.where(oss.str());
	}

	return query;
}

Release::Release(const std::string& name, const std::optional<UUID>& MBID)
: _name {std::string(name, 0 , _maxNameLength)},
_MBID {MBID ? MBID->getAsString() : ""}
{

}

std::vector<Release::pointer>
Release::getByName(Session& session, const std::string& name)
{
	session.checkUniqueLocked();

	Wt::Dbo::collection<Release::pointer> res = session.getDboSession().find<Release>().where("name = ?").bind( std::string(name, 0, _maxNameLength) );
	return std::vector<Release::pointer>(res.begin(), res.end());
}

Release::pointer
Release::getByMBID(Session& session, const UUID& mbid)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Release>().where("mbid = ?").bind(std::string {mbid.getAsString()});
}

Release::pointer
Release::getById(Session& session, IdType id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Release>().where("id = ?").bind(id);
}

Release::pointer
Release::create(Session& session, const std::string& name, const std::optional<UUID>& MBID)
{
	session.checkSharedLocked();

	Release::pointer res {session.getDboSession().add(std::make_unique<Release>(name, MBID))};
	session.getDboSession().flush();

	return res;
}

std::size_t
Release::getCount(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> releases {session.getDboSession().find<Release>()};
	return releases.size();
}

std::vector<Release::pointer>
Release::getAll(Session& session, std::optional<Range> range)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> res = session.getDboSession().find<Release>()
		.offset(range ? static_cast<int>(range->offset) : -1)
		.limit(range ? static_cast<int>(range->limit) : -1)
		.orderBy("name COLLATE NOCASE");

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<IdType>
Release::getAllIds(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<IdType> res = session.getDboSession().query<IdType>("SELECT id FROM release");
	return std::vector<IdType>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getAllOrderedByArtist(Session& session, std::optional<std::size_t> offset, std::optional<std::size_t> size)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> res = session.getDboSession().query<Wt::Dbo::ptr<Release>>(
			"SELECT DISTINCT r FROM release r"
			" INNER JOIN track t ON r.id = t.release_id"
			" INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id"
			" INNER JOIN artist a ON t_a_l.artist_id = a.id")
		 .offset(offset ? static_cast<int>(*offset) : -1)
		 .limit(size ? static_cast<int>(*size) : -1)
		 .orderBy("a.name COLLATE NOCASE, r.name COLLATE NOCASE");

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getAllRandom(Session& session, const std::set<IdType>& clusterIds, std::optional<std::size_t> size)
{
	session.checkSharedLocked();

	auto query {createQuery<Release::pointer>(session, "SELECT DISTINCT r from release r", clusterIds,{})};

	Wt::Dbo::collection<pointer> res = query
		.orderBy("RANDOM()")
		.limit(size ? static_cast<int>(*size) : -1);

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<IdType>
Release::getAllIdsRandom(Session& session, const std::set<IdType>& clusterIds, std::optional<std::size_t> size)
{
	session.checkSharedLocked();

	auto query {createQuery<IdType>(session, "SELECT DISTINCT r.id from release r", clusterIds,{})};

	Wt::Dbo::collection<IdType> res = query
		.orderBy("RANDOM()")
		.limit(size ? static_cast<int>(*size) : -1);

	return std::vector<IdType>(res.begin(), res.end());
}


std::vector<Release::pointer>
Release::getAllOrphans(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<Release::pointer> res = session.getDboSession().query<Wt::Dbo::ptr<Release>>("select r from release r LEFT OUTER JOIN Track t ON r.id = t.release_id WHERE t.id IS NULL");

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getLastWritten(Session& session,
		std::optional<Wt::WDateTime> after,
		const std::set<IdType>& clusterIds,
		std::optional<Range> range,
		bool& moreResults)
{
	session.checkSharedLocked();

	auto query {createQuery<Release::pointer>(session, "SELECT r from release r", clusterIds, {})};
	if (after)
		query.where("t.file_last_write > ?").bind(after);

	Wt::Dbo::collection<Release::pointer> collection = query
		.orderBy("t.file_last_write DESC")
		.groupBy("r.id")
		.offset(range ? static_cast<int>(range->offset) : -1)
		.limit(range ? static_cast<int>(range->limit) + 1: -1);

	auto res {std::vector<pointer>(collection.begin(), collection.end())};
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
Release::getByYear(Session& session, int yearFrom, int yearTo, std::optional<Range> range)
{
	Wt::Dbo::collection<Release::pointer> res = session.getDboSession().query<Release::pointer>
		("SELECT DISTINCT r from release r INNER JOIN track t ON r.id = t.release_id")
		.where("t.year >= ?").bind(yearFrom)
		.where("t.year <= ?").bind(yearTo)
		.orderBy("t.year, r.name COLLATE NOCASE")
		.offset(range ? static_cast<int>(range->offset) : -1)
		.limit(range ? static_cast<int>(range->limit) : -1);

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getStarred(Session& session,
		User::pointer user,
		const std::set<IdType>& clusterIds,
		std::optional<Range> range,
		bool& moreResults)
{
	session.checkSharedLocked();

	auto query {createQuery<Release::pointer>(session, "SELECT r from release r", clusterIds, {})};
	{
		std::ostringstream oss;
		oss << "r.id IN (SELECT DISTINCT r.id FROM release r"
			" INNER JOIN user_release_starred urs ON urs.release_id = r.id"
			" INNER JOIN user u ON u.id = urs.user_id WHERE u.id = ?)";

		query.bind(user.id());
		query.where(oss.str());
	}

	Wt::Dbo::collection<Release::pointer> collection = query
		.groupBy("r.id")
		.orderBy("r.name COLLATE NOCASE")
		.offset(range ? static_cast<int>(range->offset) : -1)
		.limit(range ? static_cast<int>(range->limit) + 1: -1);

	auto res {std::vector<pointer>(collection.begin(), collection.end())};
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
Release::getByClusters(Session& session, const std::set<IdType>& clusters)
{
	assert(!clusters.empty());

	session.checkSharedLocked();

	bool moreResults;
	return getByFilter(session, clusters, {}, std::nullopt, moreResults);
}

std::vector<Release::pointer>
Release::getByFilter(Session& session,
		const std::set<IdType>& clusterIds,
		const std::vector<std::string>& keywords,
		std::optional<Range> range,
		bool& moreResults)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> collection = createQuery<Release::pointer>(session, "SELECT r from release r", clusterIds, keywords)
		.groupBy("r.id")
		.orderBy("r.name COLLATE NOCASE")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1);

	auto res {std::vector<pointer>(collection.begin(), collection.end())};

	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<IdType>
Release::getAllIdsWithClusters(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<IdType> res = session.getDboSession().query<IdType>
		("SELECT DISTINCT r.id FROM release r"
		 	" INNER JOIN track t ON t.release_id = r.id"
		 	" INNER JOIN track_cluster t_c ON t_c.track_id = t.id")
		.limit(limit ? static_cast<int>(*limit) : -1);

	return std::vector<IdType>(res.begin(), res.end());
}


std::optional<std::size_t>
Release::getTotalTrack(void) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	int res = session()->query<int>("SELECT COALESCE(MAX(total_track),0) FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.bind(this->id());

	return (res > 0) ? std::make_optional<std::size_t>(res) : std::nullopt;
}

std::optional<std::size_t>
Release::getTotalDisc(void) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	int res = session()->query<int>("SELECT COALESCE(MAX(total_disc),0) FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.bind(this->id());

	return (res > 0) ? std::make_optional<std::size_t>(res) : std::nullopt;
}

std::optional<int>
Release::getReleaseYear(bool original) const
{
	assert(session());

	const std::string field {original ? "original_year" : "year"};

	Wt::Dbo::collection<int> dates = session()->query<int>(
			std::string {"SELECT "} + "t." + field + " FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.groupBy(field)
		.bind(this->id());

	// various dates => no date
	if (dates.empty() || dates.size() > 1)
		return std::nullopt;

	auto date {dates.front()};

	if (date > 0)
		return date;
	else
		return std::nullopt;
}

std::optional<std::string>
Release::getCopyright() const
{
	assert(session());

	Wt::Dbo::collection<std::string> copyrights = session()->query<std::string>
		("SELECT copyright FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.groupBy("copyright")
		.bind(this->id());

	std::vector<std::string> values(copyrights.begin(), copyrights.end());

	// various copyrights => no copyright
	if (values.empty() || values.size() > 1 || values.front().empty())
		return std::nullopt;

	return values.front();
}

std::optional<std::string>
Release::getCopyrightURL() const
{
	assert(session());

	Wt::Dbo::collection<std::string> copyrights = session()->query<std::string>
		("SELECT copyright_url FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.groupBy("copyright_url")
		.bind(this->id());

	std::vector<std::string> values(copyrights.begin(), copyrights.end());

	// various copyright URLs => no copyright URL
	if (values.empty() || values.size() > 1 || values.front().empty())
		return std::nullopt;

	return values.front();
}

std::vector<Wt::Dbo::ptr<Artist>>
Release::getArtists(TrackArtistLink::Type linkType) const
{
	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	Wt::Dbo::collection<Wt::Dbo::ptr<Artist>> res = session()->query<Wt::Dbo::ptr<Artist>>(
			"SELECT DISTINCT a FROM artist a"
			" INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
			" INNER JOIN track t ON t.id = t_a_l.track_id"
			" INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?").bind(self()->id())
		.where("t_a_l.type = ?").bind(linkType);

	return std::vector<Wt::Dbo::ptr<Artist>>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getSimilarReleases(std::optional<std::size_t> offset, std::optional<std::size_t> count) const
{
	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	Wt::Dbo::Query<pointer> query {session()->query<pointer>(
			"SELECT r FROM release r"
			" INNER JOIN track t ON t.release_id = r.id"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
				" WHERE "
					" t_c.cluster_id IN (SELECT c.id from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN release r ON r.id = t.release_id WHERE r.id = ?)"
					" AND r.id <> ?"
				)
		.bind(self()->id())
		.bind(self()->id())
		.groupBy("r.id")
		.orderBy("COUNT(*) DESC, RANDOM()")
		.limit(count ? static_cast<int>(*count) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1)};

	Wt::Dbo::collection<pointer> res = query;
	return std::vector<pointer>(res.begin(), res.end());
}

bool
Release::hasVariousArtists() const
{
	// TODO optimize
	return getArtists().size() > 1;
}

std::vector<Wt::Dbo::ptr<Track>>
Release::getTracks(const std::set<IdType>& clusterIds) const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<Release>::invalidId() );
	assert(session());

	WhereClause where;

	std::ostringstream oss;
	oss << "SELECT t FROM track t INNER JOIN release r ON t.release_id = r.id";

	if (!clusterIds.empty())
	{
		oss << " INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;

		for (auto id : clusterIds)
			clusterClause.Or(WhereClause("c.id = ?")).bind(std::to_string(id));

		where.And(clusterClause);
	}

	where.And(WhereClause("r.id = ?")).bind(std::to_string(id()));

	oss << " " << where.get();

	if (!clusterIds.empty())
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size();

	oss << " ORDER BY t.disc_number,t.track_number";

	Wt::Dbo::Query<Track::pointer> query = session()->query<Track::pointer>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
	{
		query.bind(bindArg);
	}

	Wt::Dbo::collection< Wt::Dbo::ptr<Track> > res = query;

	return std::vector< Wt::Dbo::ptr<Track> > (res.begin(), res.end());
}

std::size_t
Release::getTracksCount() const
{
	return _tracks.size();
}

Wt::Dbo::ptr<Track>
Release::getFirstTrack() const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<Artist>::invalidId());
	assert(session());

	return session()->query<Track::pointer>("SELECT t from track t")
		.join("release r ON t.release_id = r.id")
		.where("r.id = ?").bind(self()->id())
		.orderBy("t.disc_number,t.track_number")
		.limit(1);
}

std::chrono::milliseconds
Release::getDuration() const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<Artist>::invalidId());
	assert(session());

	using milli = std::chrono::duration<int, std::milli>;

	Wt::Dbo::Query<milli> query {session()->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t INNER JOIN release r ON t.release_id = r.id")
			.where("r.id = ?").bind(self()->id())};

	return query.resultValue();
}

Wt::WDateTime
Release::getLastWritten() const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<Artist>::invalidId());
	assert(session());

	Wt::Dbo::Query<Wt::WDateTime> query {session()->query<Wt::WDateTime>("SELECT COALESCE(MAX(file_last_write), '1970-01-01T00:00:00') FROM track t INNER JOIN release r ON t.release_id = r.id")
			.where("r.id = ?").bind(self()->id())};

	return query.resultValue();
}

std::vector<std::vector<Wt::Dbo::ptr<Cluster>>>
Release::getClusterGroups(std::vector<ClusterType::pointer> clusterTypes, std::size_t size) const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<Artist>::invalidId() );
	assert(session());

	WhereClause where;

	std::ostringstream oss;

	oss << "SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN cluster_type c_type ON c.cluster_type_id = c_type.id INNER JOIN release r ON t.release_id = r.id ";

	where.And(WhereClause("r.id = ?")).bind(std::to_string(self()->id()));
	{
		WhereClause clusterClause;
		for (auto clusterType : clusterTypes)
			clusterClause.Or(WhereClause("c_type.id = ?")).bind(std::to_string(clusterType.id()));
		where.And(clusterClause);
	}
	oss << " " << where.get();
	oss << " GROUP BY c.id ORDER BY COUNT(c.id) DESC";

	Wt::Dbo::Query<Cluster::pointer> query = session()->query<Cluster::pointer>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
		query.bind(bindArg);

	Wt::Dbo::collection<Cluster::pointer> queryRes = query;

	std::map<IdType, std::vector<Cluster::pointer>> clusters;
	for (auto cluster : queryRes)
	{
		if (clusters[cluster->getType().id()].size() < size)
			clusters[cluster->getType().id()].push_back(cluster);
	}

	std::vector<std::vector<Cluster::pointer>> res;
	for (auto cluster_list : clusters)
		res.push_back(cluster_list.second);

	return res;
}

} // namespace Database
