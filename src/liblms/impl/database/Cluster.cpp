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

#include "database/Cluster.hpp"

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "SqlQuery.hpp"

namespace Database {

Cluster::Cluster()
{
}

Cluster::Cluster(Wt::Dbo::ptr<ClusterType> type, std::string name)
	: _name(std::string(name, 0, _maxNameLength)),
	_clusterType(type)
{
}

Cluster::pointer
Cluster::create(Session& session, Wt::Dbo::ptr<ClusterType> type, std::string name)
{
	session.checkUniqueLocked();

	Cluster::pointer res {session.getDboSession().add(std::make_unique<Cluster>(type, name))};
	session.getDboSession().flush();

	return res;
}

std::vector<Cluster::pointer>
Cluster::getAll(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<Cluster::pointer> res {session.getDboSession().find<Cluster>()};

	return std::vector<Cluster::pointer>(res.begin(), res.end());
}

std::vector<Cluster::pointer>
Cluster::getAllOrphans(Session& session)
{
	session.checkSharedLocked();
	Wt::Dbo::collection<Cluster::pointer> res {session.getDboSession().query<Cluster::pointer>("SELECT DISTINCT c FROM cluster c WHERE NOT EXISTS(SELECT 1 FROM track_cluster t_c WHERE t_c.cluster_id = c.id)")};

	return std::vector<Cluster::pointer>(res.begin(), res.end());
}

Cluster::pointer
Cluster::getById(Session& session, IdType id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Cluster>().where("id = ?").bind(id);
}

void
Cluster::addTrack(Wt::Dbo::ptr<Track> track)
{
	_tracks.insert(track);
}

std::vector<Wt::Dbo::ptr<Track>>
Cluster::getTracks(std::optional<std::size_t> offset, std::optional<std::size_t> limit) const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<Track::pointer> res
		{session()->query<Track::pointer>("SELECT t FROM track t INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id")
			.where("c.id = ?").bind(self()->id())
			.offset(offset ? static_cast<int>(*offset) : -1)
			.limit(limit ? static_cast<int>(*limit) : -1)};

	return std::vector<Wt::Dbo::ptr<Track>>(res.begin(), res.end());
}

std::set<IdType>
Cluster::getTrackIds() const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	Wt::Dbo::collection<IdType> res = session()->query<IdType>("SELECT t_c.track_id FROM track_cluster t_c INNER JOIN cluster c ON c.id = t_c.cluster_id")
		.where("c.id = ?").bind(self()->id());

	return std::set<IdType>(res.begin(), res.end());

}

std::size_t
Cluster::getReleasesCount() const
{
	assert(session());
	assert(IdIsValid(self()->id()));

	return session()->query<int>("SELECT COUNT(DISTINCT r.id) FROM release r INNER JOIN track t on t.release_id = r.id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id")
		.where("c.id = ?").bind(self()->id());

}


ClusterType::ClusterType(std::string name)
	: _name(name)
{
}

std::vector<ClusterType::pointer>
ClusterType::getAllOrphans(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> res = session.getDboSession().query<Wt::Dbo::ptr<ClusterType>>("select c_t from cluster_type c_t LEFT OUTER JOIN cluster c ON c_t.id = c.cluster_type_id WHERE c.id IS NULL");

	return std::vector<pointer>(res.begin(), res.end());
}


ClusterType::pointer
ClusterType::getByName(Session& session, const std::string& name)
{
	session.checkSharedLocked();

	return session.getDboSession().find<ClusterType>().where("name = ?").bind(name);
}

ClusterType::pointer
ClusterType::getById(Session& session, IdType id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<ClusterType>().where("id= ?").bind(id);
}

std::vector<ClusterType::pointer>
ClusterType::getAll(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> res = session.getDboSession().find<ClusterType>();

	return std::vector<pointer>(res.begin(), res.end());
}

ClusterType::pointer
ClusterType::create(Session& session, const std::string& name)
{
	session.checkUniqueLocked();

	ClusterType::pointer res {session.getDboSession().add(std::make_unique<ClusterType>(name))};
	session.getDboSession().flush();

	return res;
}

Cluster::pointer
ClusterType::getCluster(const std::string& name) const
{
	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	return session()->find<Cluster>()
		.where("name = ?").bind(name)
		.where("cluster_type_id = ?").bind(self()->id());
}

std::vector<Cluster::pointer>
ClusterType::getClusters() const
{
	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	Wt::Dbo::collection<Cluster::pointer> res = session()->find<Cluster>()
						.where("cluster_type_id = ?").bind(self()->id())
						.orderBy("name");

	return std::vector<Cluster::pointer>(res.begin(), res.end());
}


} // namespace Database

