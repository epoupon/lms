/*
 * Copyright (C) 2013-2016 Emeric Poupon
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

#include <boost/property_tree/json_parser.hpp>

#include <Wt/Dbo/QueryModel>

#include "utils/Logger.hpp"

#include "SqlQuery.hpp"

#include "Types.hpp"

namespace Database {

Track::Track(const boost::filesystem::path& p)
:
_trackNumber(0),
_totalTrackNumber(0),
_discNumber(0),
_totalDiscNumber(0),
_filePath( p.string() ),
_coverType(CoverType::None)
{
}

Wt::Dbo::collection< Track::pointer >
Track::getAll(Wt::Dbo::Session& session)
{
	return session.find<Track>();
}

std::vector<Track::id_type>
Track::getAllIds(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction(session);
	Wt::Dbo::collection<Track::id_type> res = session.query<Track::id_type>("SELECT id from track");
	return std::vector<Track::id_type>(res.begin(), res.end());
}

Track::pointer
Track::getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.find<Track>().where("file_path = ?").bind(p.string());
}

Track::pointer
Track::getById(Wt::Dbo::Session& session, id_type id)
{
	return session.find<Track>().where("id = ?").bind(id);
}

Track::pointer
Track::getByMBID(Wt::Dbo::Session& session, const std::string& mbid)
{
	return session.find<Track>().where("mbid = ?").bind(mbid);
}

Track::pointer
Track::create(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.add(new Track(p) );
}

std::vector<boost::filesystem::path>
Track::getAllPaths(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction(session);
	Wt::Dbo::collection<std::string> res = session.query<std::string>("SELECT file_path from track");
	return std::vector<boost::filesystem::path>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getMBIDDuplicates(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.query<pointer>( "SELECT track FROM track WHERE mbid in (SELECT mbid FROM track WHERE mbid <> '' GROUP BY mbid HAVING COUNT (*) > 1)").orderBy("track.release_id,track.disc_number,track.track_number,track.mbid");
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getChecksumDuplicates(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.query<pointer>( "SELECT track FROM track WHERE checksum in (SELECT checksum FROM track WHERE Length(checksum) > 0 GROUP BY checksum HAVING COUNT(*) > 1)").orderBy("track.release_id,track.disc_number,track.track_number,track.checksum");
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector< Cluster::pointer >
Track::getClusters(void) const
{
	std::vector< Cluster::pointer > clusters;
	std::copy(_clusters.begin(), _clusters.end(), std::back_inserter(clusters));
	return clusters;
}

static
Wt::Dbo::Query< Track::pointer >
getQuery(Wt::Dbo::Session& session,
		const std::set<id_type>& clusterIds,
		const std::vector<std::string> keywords)
{
	WhereClause where;

	std::ostringstream oss;
	oss << "SELECT t FROM track t";

	for (auto keyword : keywords)
		where.And(WhereClause("t.name LIKE ?")).bind("%%" + keyword + "%%");

	if (!clusterIds.empty())
	{
		oss << " INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;

		for (auto id : clusterIds)
			clusterClause.Or(WhereClause("c.id = ?")).bind(std::to_string(id));

		where.And(clusterClause);
	}

	oss << " " << where.get();

	if (!clusterIds.empty())
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size();

	oss << " ORDER BY t.name";

	Wt::Dbo::Query<Track::pointer> query = session.query<Track::pointer>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
		query.bind(bindArg);

	return query;
}

Track::StatsQueryResult
Track::getStats(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = filter.generatePartialQuery();

	Wt::Dbo::Query<StatsQueryResult> query = session.query<StatsQueryResult>( "SELECT COUNT(\"id\"), SUM(\"dur\") FROM  (SELECT t.id as \"id\", t.duration as \"dur\" FROM track t INNER JOIN artist a ON t.artist_id = a.id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN release r ON r.id = t.release_id " + sqlQuery.where().get() + " GROUP BY t.id)");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}


std::vector<Track::pointer>
Track::getByFilter(Wt::Dbo::Session& session,
		const std::set<id_type>& clusterIds,
		const std::vector<std::string> keywords,
		int offset, int size, bool& moreResults)
{
	Wt::Dbo::collection<pointer> collection = getQuery(session, clusterIds, keywords)
		.limit(size != -1 ? size + 1 : -1)
		.offset(offset);

	auto res = std::vector<pointer>(collection.begin(), collection.end());

	if (size != -1 && res.size() == static_cast<std::size_t>(size) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Track::pointer>
Track::getByFilter(Wt::Dbo::Session& session,
			const std::set<id_type>& clusters)
{
	bool moreResults;

	return getByFilter(session, clusters, std::vector<std::string>(), -1, -1, moreResults);
}

boost::optional<std::size_t>
Track::getTrackNumber(void) const
{
	return (_trackNumber > 0) ? boost::make_optional<std::size_t>(_trackNumber) : boost::none;
}

boost::optional<std::size_t>
Track::getTotalTrackNumber(void) const
{
	return (_totalTrackNumber > 0) ? boost::make_optional<std::size_t>(_totalTrackNumber) : boost::none;
}

boost::optional<std::size_t>
Track::getDiscNumber(void) const
{
	return (_discNumber > 0) ? boost::make_optional<std::size_t>(_discNumber) : boost::none;
}

boost::optional<std::size_t>
Track::getTotalDiscNumber(void) const
{
	return (_totalDiscNumber > 0) ? boost::make_optional<std::size_t>(_totalDiscNumber) : boost::none;
}


Cluster::Cluster()
{
}

Cluster::Cluster(Wt::Dbo::ptr<ClusterType> type, std::string name)
	: _name(std::string(name, 0, _maxNameLength)),
	_clusterType(type)
{
}

Cluster::pointer
Cluster::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<ClusterType> type, std::string name)
{
	return session.add(new Cluster(type, name));
}

std::vector<Cluster::pointer>
Cluster::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<Cluster::pointer> res = session.find<Cluster>();

	return std::vector<Cluster::pointer>(res.begin(), res.end());
}

Wt::Dbo::Query<Cluster::pointer>
Cluster::getQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = filter.generatePartialQuery();

	Wt::Dbo::Query<pointer> query
		= session.query<pointer>( "SELECT g FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id INNER JOIN artist a ON t.artist_id = a.id INNER JOIN release r ON r.id = t.release_id INNER JOIN track t ON t.id = t_c.track_id " + sqlQuery.where().get()).groupBy("c.name").orderBy("c.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

std::vector<Cluster::pointer>
Cluster::getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	Wt::Dbo::collection<pointer> res = getQuery(session, filter).limit(size).offset(offset);

	return std::vector<pointer>(res.begin(), res.end());
}

ClusterType::ClusterType(std::string name)
	: _name(name)
{
}

std::vector<ClusterType::pointer>
ClusterType::getAllOrphans(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.query<Wt::Dbo::ptr<ClusterType>>("select c_t from cluster_type c_t LEFT OUTER JOIN cluster c ON c_t.id = c.cluster_type_id WHERE c.id IS NULL");

	return std::vector<pointer>(res.begin(), res.end());
}


ClusterType::pointer
ClusterType::getByName(Wt::Dbo::Session& session, std::string name)
{
	return session.find<ClusterType>().where("name = ?").bind(name);
}

std::vector<ClusterType::pointer>
ClusterType::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.find<ClusterType>();

	return std::vector<pointer>(res.begin(), res.end());
}

ClusterType::pointer
ClusterType::create(Wt::Dbo::Session& session, std::string name)
{
	return session.add(new ClusterType(name));
}

Cluster::pointer
ClusterType::getCluster(std::string name) const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<ClusterType>::invalidId() );
	assert(session());

	return session()->find<Cluster>()
		.where("name = ?").bind(name)
		.where("cluster_type_id = ?").bind(self()->id());
}

std::vector<Cluster::pointer>
ClusterType::getClusters() const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<Release>::invalidId() );
	assert(session());

	Wt::Dbo::collection<Cluster::pointer> res = session()->find<Cluster>()
						.where("cluster_type_id = ?").bind(self()->id())
						.orderBy("name");

	return std::vector<Cluster::pointer>(res.begin(), res.end());
}

} // namespace Database

