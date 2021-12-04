/*
 * Copyright (C) 2018 Emeric Poupon
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

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "services/database/Object.hpp"
#include "services/database/ClusterId.hpp"
#include "services/database/TrackId.hpp"
#include "services/database/Types.hpp"

namespace Database {

class Track;
class ClusterType;
class ScanSettings;
class Session;

class Cluster : public Object<Cluster, ClusterId>
{
	public:
		Cluster() = default;
		Cluster(ObjectPtr<ClusterType> type, std::string_view name);

		// Find utility
		static std::size_t				getCount(Session& session);
		static RangeResults<ClusterId>	find(Session& session, Range range);
		static pointer					find(Session& session, ClusterId id);
		static RangeResults<ClusterId>	findOrphans(Session& session, Range range);

		// Create utility
		static pointer create(Session& session, ObjectPtr<ClusterType> type, std::string_view name);

		// Accessors
		const std::string&				getName() const		{ return _name; }
		ObjectPtr<ClusterType>			getType() const	{ return _clusterType; }
		std::size_t						getTracksCount() const		{ return _tracks.size(); }
		RangeResults<TrackId>			getTracks(Range range) const;
		std::size_t						getReleasesCount() const;

		void addTrack(ObjectPtr<Track> track);

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _name,	"name");

			Wt::Dbo::belongsTo(a, _clusterType, "cluster_type", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
		}

	private:
		static const std::size_t _maxNameLength = 128;

		std::string	_name;

		Wt::Dbo::ptr<ClusterType> _clusterType;
		Wt::Dbo::collection< Wt::Dbo::ptr<Track> > _tracks;
};


class ClusterType : public Object<ClusterType, ClusterTypeId>
{
	public:
		ClusterType() = default;
		ClusterType(std::string_view name);

		// Getters
		static std::size_t					getCount(Session& session);
		static RangeResults<ClusterTypeId>	find(Session& session, Range range);
		static pointer 						find(Session& session, const std::string& name);
		static pointer						find(Session& session, ClusterTypeId id);
		static RangeResults<ClusterTypeId>	findOrphans(Session& session, Range range);
		static RangeResults<ClusterTypeId>	findUsed(Session& session, Range range);

		static pointer create(Session& session, const std::string& name);
		static void remove(Session& session, const std::string& name);

		// Accessors
		const std::string&				getName() const { return _name; }
		std::vector<Cluster::pointer>	getClusters() const;
		Cluster::pointer				getCluster(const std::string& name) const;

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _name,	"name");
			Wt::Dbo::hasMany(a, _clusters, Wt::Dbo::ManyToOne, "cluster_type");
			Wt::Dbo::belongsTo(a, _scanSettings, "scan_settings", Wt::Dbo::OnDeleteCascade);
		}

	private:

		static const std::size_t _maxNameLength = 128;

		std::string     _name;
		Wt::Dbo::collection< Wt::Dbo::ptr<Cluster> > _clusters;
		Wt::Dbo::ptr<ScanSettings> _scanSettings;
};

} // namespace Database

