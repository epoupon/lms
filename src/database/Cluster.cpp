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

#include "Cluster.hpp"

#include "Artist.hpp"
#include "Release.hpp"
#include "ScanSettings.hpp"
#include "SqlQuery.hpp"
#include "Track.hpp"

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
Cluster::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<ClusterType> type, std::string name)
{
	return session.add(std::make_unique<Cluster>(type, name));
}

std::vector<Cluster::pointer>
Cluster::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<Cluster::pointer> res = session.find<Cluster>();

	return std::vector<Cluster::pointer>(res.begin(), res.end());
}

Cluster::pointer
Cluster::getById(Wt::Dbo::Session& session, IdType id)
{
	return session.find<Cluster>().where("id = ?").bind(id);
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
	return session.add(std::make_unique<ClusterType>(name));
}

Cluster::pointer
ClusterType::getCluster(std::string name) const
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

void
Cluster::addTrack(Wt::Dbo::ptr<Track> track)
{
	_tracks.insert(track);
}

} // namespace Database

