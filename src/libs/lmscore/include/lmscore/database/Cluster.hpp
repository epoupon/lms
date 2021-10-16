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

#include "lmscore/database/Types.hpp"

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
		static std::vector<pointer> getAll(Session& session);
		static std::vector<pointer> getAllOrphans(Session& session);
		static pointer getById(Session& session, ClusterId id);

		// Create utility
		static pointer create(Session& session, ObjectPtr<ClusterType> type, std::string_view name);

		// Accessors
		const std::string& getName() const		{ return _name; }
		ObjectPtr<ClusterType> getType() const	{ return _clusterType; }
		std::size_t getTracksCount() const		{ return _tracks.size(); }
		std::vector<ObjectPtr<Track>> getTracks(std::optional<std::size_t> offset = {}, std::optional<std::size_t> limit = {}) const;
		std::vector<TrackId> getTrackIds() const;
		std::size_t getReleasesCount() const;

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
		static std::vector<pointer> getAllOrphans(Session& session);
		static std::vector<pointer> getAllUsed(Session& session);
		static pointer getByName(Session& session, const std::string& name);
		static pointer getById(Session& session, ClusterTypeId id);
		static std::vector<pointer> getAll(Session& session);

		static pointer create(Session& session, const std::string& name);
		static void remove(Session& session, const std::string& name);

		// Accessors
		const std::string& getName(void) const { return _name; }
		std::vector<Cluster::pointer> getClusters() const;
		Cluster::pointer getCluster(const std::string& name) const;

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

