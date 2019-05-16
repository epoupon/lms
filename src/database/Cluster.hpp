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
#include <vector>

#include <Wt/Dbo/Dbo.h>

#include <Wt/WDateTime.h>

#include "Types.hpp"

namespace Database {

class Track;
class ClusterType;
class ScanSettings;

class Cluster : public Wt::Dbo::Dbo<Cluster>
{
	public:
		using pointer = Wt::Dbo::ptr<Cluster>;

		Cluster();
		Cluster(Wt::Dbo::ptr<ClusterType> type, std::string name);

		// Find utility
		static std::vector<pointer> getAll(Wt::Dbo::Session& session);
		static std::vector<pointer> getAllOrphans(Wt::Dbo::Session& session);
		static pointer getById(Wt::Dbo::Session& session, IdType id);

		// Create utility
		static pointer create(Wt::Dbo::Session& session, Wt::Dbo::ptr<ClusterType> type, std::string name);

		// Accessors
		const std::string& getName() const { return _name; }
		Wt::Dbo::ptr<ClusterType> getType() const { return _clusterType; }
		std::size_t getCount() const { return _tracks.size(); }
		std::vector<Wt::Dbo::ptr<Track>> getTracks(int offset, int limit) const;
		std::set<IdType> getTrackIds() const;

		void addTrack(Wt::Dbo::ptr<Track> track);

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


class ClusterType : public Wt::Dbo::Dbo<ClusterType>
{
	public:

		using pointer = Wt::Dbo::ptr<ClusterType>;

		ClusterType() {}
		ClusterType(std::string name);

		static std::vector<pointer> getAllOrphans(Wt::Dbo::Session& session);
		static pointer getByName(Wt::Dbo::Session& session, std::string name);
		static pointer getById(Wt::Dbo::Session& session, IdType id);
		static std::vector<pointer> getAll(Wt::Dbo::Session& session);

		static pointer create(Wt::Dbo::Session& session, std::string name);
		static void remove(Wt::Dbo::Session& session, std::string name);

		// Accessors
		const std::string& getName(void) const { return _name; }
		std::vector<Cluster::pointer> getClusters() const;
		Cluster::pointer getCluster(std::string name) const;

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

