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

#include "SqlQuery.hpp"

#include "utils/Logger.hpp"

#include "Release.hpp"
#include "DbArtist.hpp"
#include "Track.hpp"

namespace Database
{

Artist::Artist(const std::string& name, const std::string& MBID)
: _name(std::string(name, 0 , _maxNameLength)),
_MBID(MBID)
{

}

std::vector<Artist::pointer>
Artist::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	Wt::Dbo::collection<Artist::pointer> res = session.find<Artist>().where("name = ?").bind( std::string(name, 0, _maxNameLength) );
	return std::vector<Artist::pointer>(res.begin(), res.end());
}

Artist::pointer
Artist::getByMBID(Wt::Dbo::Session& session, const std::string& mbid)
{
	return session.find<Artist>().where("mbid = ?").bind(mbid);
}

Artist::pointer
Artist::getById(Wt::Dbo::Session& session, IdType id)
{
	return session.find<Artist>().where("id = ?").bind(id);
}

Artist::pointer
Artist::create(Wt::Dbo::Session& session, const std::string& name, const std::string& MBID)
{
	return session.add(std::make_unique<Artist>(name, MBID));
}

std::vector<Artist::pointer>
Artist::getAll(Wt::Dbo::Session& session, int offset, int size)
{
	Wt::Dbo::collection<pointer> res = session.find<Artist>().orderBy("LOWER(name)").offset(offset).limit(size);
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Artist::pointer>
Artist::getAllOrphans(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<Artist::pointer> res = session.query< Wt::Dbo::ptr<Artist> >("SELECT DISTINCT a FROM artist a LEFT OUTER JOIN Track t ON a.id = t.artist_id WHERE t.id IS NULL");

	return std::vector<pointer>(res.begin(), res.end());
}

static
Wt::Dbo::Query<Artist::pointer>
getQuery(Wt::Dbo::Session& session,
		const std::set<IdType>& clusterIds,
		const std::vector<std::string>& keywords)
{
	WhereClause where;

	std::ostringstream oss;
	oss << "SELECT DISTINCT a FROM artist a";

	for (auto keyword : keywords)
		where.And(WhereClause("a.name LIKE ?")).bind("%%" + keyword + "%%");

	if (!clusterIds.empty())
	{
		oss << " INNER JOIN track t ON t.artist_id = a.id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;

		for (auto id : clusterIds)
			clusterClause.Or(WhereClause("c.id = ?")).bind(std::to_string(id));

		where.And(clusterClause);
	}
	oss << " " << where.get();

	if (!clusterIds.empty())
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size();

	oss << " ORDER BY a.name";

	Wt::Dbo::Query<Artist::pointer> query = session.query<Artist::pointer>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
	{
		query.bind(bindArg);
	}

	return query;
}

std::vector<Artist::pointer>
Artist::getByFilter(Wt::Dbo::Session& session,
		const std::set<IdType>& clusters,
		const std::vector<std::string> keywords,
		int offset, int size, bool& moreResults)
{
	Wt::Dbo::collection<Artist::pointer> collection = getQuery(session, clusters, keywords)
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

std::vector<Wt::Dbo::ptr<Release>>
Artist::getReleases(const std::set<IdType>& clusterIds) const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<Artist>::invalidId() );
	assert(session());

	WhereClause where;

	std::ostringstream oss;
	oss << "SELECT DISTINCT r FROM release r INNER JOIN artist a ON t.artist_id = a.id INNER JOIN track t ON t.release_id = r.id";

	if (!clusterIds.empty())
	{
		oss << " INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;

		for (auto id : clusterIds)
			clusterClause.Or(WhereClause("c.id = ?")).bind(std::to_string(id));

		where.And(clusterClause);
	}

	where.And(WhereClause("a.id = ?")).bind(std::to_string(id()));

	oss << " " << where.get();

	if (!clusterIds.empty())
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size();

	oss << " ORDER BY t.year,r.name";

	Wt::Dbo::Query<Release::pointer> query = session()->query<Release::pointer>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
	{
		query.bind(bindArg);
	}

	Wt::Dbo::collection< Wt::Dbo::ptr<Release> > res = query;

	return std::vector< Wt::Dbo::ptr<Release> > (res.begin(), res.end());
}

std::vector<std::vector<Wt::Dbo::ptr<Cluster>>>
Artist::getClusterGroups(std::vector<ClusterType::pointer> clusterTypes, std::size_t size) const
{
	assert(self());
	assert(self()->id() != Wt::Dbo::dbo_traits<Artist>::invalidId() );
	assert(session());

	WhereClause where;

	std::ostringstream oss;
	oss << "SELECT  c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN cluster_type c_type ON c.cluster_type_id = c_type.id INNER JOIN artist a ON t.artist_id  = a.id";

	where.And(WhereClause("a.id = ?")).bind(std::to_string(self()->id()));
	{
		WhereClause clusterClause;
		for (auto clusterType : clusterTypes)
			clusterClause.Or(WhereClause("c_type.id = ?")).bind(std::to_string(clusterType.id()));

		where.And(clusterClause);
	}
	oss << " " << where.get();
	oss << "GROUP BY c.id ORDER BY COUNT(c.id) DESC";

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
