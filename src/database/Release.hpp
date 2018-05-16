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

#pragma once

#include <boost/optional.hpp>

#include <Wt/Dbo/Dbo.h>

#include "Types.hpp"

namespace Database
{

class Track;
class Release;
class Artist;
class Cluster;
class ClusterType;

class Release : public Wt::Dbo::Dbo<Release>
{
	public:

		typedef Wt::Dbo::ptr<Release> pointer;

		Release() {}
		Release(const std::string& name, const std::string& MBID = "");

		// Accessors
		static pointer			getByMBID(Wt::Dbo::Session& session, const std::string& MBID);
		static std::vector<pointer>	getByName(Wt::Dbo::Session& session, const std::string& name);
		static pointer			getById(Wt::Dbo::Session& session, IdType id);
		static std::vector<pointer>	getAllOrphans(Wt::Dbo::Session& session); // no track related
		static std::vector<pointer>	getAll(Wt::Dbo::Session& session, int offset, int size);

		static std::vector<pointer>	getByFilter(Wt::Dbo::Session& session,
							const std::set<IdType>& clusters,           // at least one track that belongs to these clusters
							const std::vector<std::string> keywords,        // name must match all of these keywords
							int offset,
							int size,
							bool& moreExpected);

		std::vector<Wt::Dbo::ptr<Track>> getTracks(const std::set<IdType>& clusters = std::set<IdType>()) const;
		// Get the cluster of the tracks that belong to this release
		// Each clusters are grouped by cluster type, sorted by the number of occurence
		// size is the max number of cluster per cluster type
		std::vector<std::vector<Wt::Dbo::ptr<Cluster>>> getClusterGroups(std::vector<Wt::Dbo::ptr<ClusterType>> clusterTypes, std::size_t size) const;

		// Create
		static pointer create(Wt::Dbo::Session& session, const std::string& name, const std::string& MBID = "");

		// Utility functions
		boost::optional<int> getReleaseYear(bool originalDate = false) const; // 0 if unknown or various

		// Accessors
		std::string	getName() const		{ return _name; }
		std::string	getMBID() const		{ return _MBID; }
		std::chrono::seconds getDuration(void) const;

		// Get the artists of this release
		std::vector<Wt::Dbo::ptr<Artist> > getArtists() const;
		bool hasVariousArtists() const;

		void setMBID(std::string mbid) { _MBID = mbid; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name, "name");
				Wt::Dbo::field(a, _MBID, "mbid");

				Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "release");
			}

	private:
		static const std::size_t _maxNameLength = 128;

		std::string _name;
		std::string _MBID;

		Wt::Dbo::collection< Wt::Dbo::ptr<Track> > _tracks; // Tracks in the release
};

} // namespace Database


