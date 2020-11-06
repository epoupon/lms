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

#include <optional>

#include <Wt/Dbo/WtSqlTraits.h>

#include "utils/UUID.hpp"
#include "TrackArtistLink.hpp"
#include "Types.hpp"

namespace Database
{

class Artist;
class Cluster;
class ClusterType;
class Release;
class Track;
class User;

class Release : public Wt::Dbo::Dbo<Release>
{
	public:

		using pointer = Wt::Dbo::ptr<Release>;

		Release() {}
		Release(const std::string& name, const std::optional<UUID>& MBID = {});

		// Accessors
		static std::size_t		getCount(Session& session);
		static pointer			getByMBID(Session& session, const UUID& MBID);
		static std::vector<pointer>	getByName(Session& session, const std::string& name);
		static pointer			getById(Session& session, IdType id);
		static std::vector<pointer>	getAllOrphans(Session& session); // no track related
		static std::vector<pointer>	getAll(Session& session, std::optional<Range> range = std::nullopt);
		static std::vector<IdType>	getAllIds(Session& session);
		static std::vector<pointer>	getAllOrderedByArtist(Session& session, std::optional<std::size_t> offset = {}, std::optional<std::size_t> size = {});
		static std::vector<pointer>	getAllRandom(Session& session, const std::set<IdType>& clusters, std::optional<std::size_t> size = {});
		static std::vector<IdType>	getAllIdsRandom(Session& session, const std::set<IdType>& clusters, std::optional<std::size_t> size = {});
		static std::vector<pointer>	getLastWritten(Session& session, std::optional<Wt::WDateTime> after, const std::set<IdType>& clusters, std::optional<Range> range, bool& moreResults);
		static std::vector<pointer>	getByYear(Session& session, int yearFrom, int yearTo, std::optional<Range> range = std::nullopt);
		static std::vector<pointer> getStarred(Session& session, Wt::Dbo::ptr<User> user, const std::set<IdType>& clusters, std::optional<Range> range, bool& moreResults);

		static std::vector<pointer>	getByClusters(Session& session, const std::set<IdType>& clusters);
		static std::vector<pointer>	getByFilter(Session& session,
							const std::set<IdType>& clusters,		// if non empty, at least one release that belongs to these clusters
							const std::vector<std::string>& keywords,	// if non empty, name must match all of these keywords
							std::optional<Range> range,
							bool& moreExpected);
		static std::vector<IdType>	getAllIdsWithClusters(Session& session, std::optional<std::size_t> limit = {});

		std::vector<Wt::Dbo::ptr<Track>> getTracks(const std::set<IdType>& clusters = std::set<IdType>()) const;
		std::size_t			getTracksCount() const;
		Wt::Dbo::ptr<Track>			getFirstTrack() const;

		// Get the cluster of the tracks that belong to this release
		// Each clusters are grouped by cluster type, sorted by the number of occurence (max to min)
		// size is the max number of cluster per cluster type
		std::vector<std::vector<Wt::Dbo::ptr<Cluster>>> getClusterGroups(std::vector<Wt::Dbo::ptr<ClusterType>> clusterTypes, std::size_t size) const;

		// Create
		static pointer create(Session& session, const std::string& name, const std::optional<UUID>& MBID = {});

		// Utility functions
		std::optional<int> getReleaseYear(bool originalDate = false) const; // 0 if unknown or various
		std::optional<std::string> getCopyright() const;
		std::optional<std::string> getCopyrightURL() const;

		// Accessors
		const std::string&		getName() const		{ return _name; }
		std::optional<UUID>		getMBID() const		{ return UUID::fromString(_MBID); }
		std::optional<std::size_t>	getTotalTrack() const;
		std::optional<std::size_t>	getTotalDisc() const;
		std::chrono::milliseconds	getDuration() const;
		Wt::WDateTime				getLastWritten() const;

		// Get the artists of this release
		std::vector<Wt::Dbo::ptr<Artist> > getArtists(TrackArtistLink::Type type = TrackArtistLink::Type::Artist) const;
		std::vector<Wt::Dbo::ptr<Artist> > getReleaseArtists() const { return getArtists(TrackArtistLink::Type::ReleaseArtist); }
		bool hasVariousArtists() const;
		std::vector<pointer>		getSimilarReleases(std::optional<std::size_t> offset = {}, std::optional<std::size_t> count = {}) const;

		void setName(std::string_view name)		{ _name = name; }
		void setMBID(const std::optional<UUID>& mbid)	{ _MBID = mbid ? mbid->getAsString() : ""; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name, "name");
				Wt::Dbo::field(a, _MBID, "mbid");

				Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "release");
				Wt::Dbo::hasMany(a, _starringUsers, Wt::Dbo::ManyToMany, "user_release_starred", "", Wt::Dbo::OnDeleteCascade);
			}

	private:
		static const std::size_t _maxNameLength {128};

		std::string	_name;
		std::string	_MBID;

		Wt::Dbo::collection<Wt::Dbo::ptr<Track>>	_tracks; // Tracks in the release
		Wt::Dbo::collection<Wt::Dbo::ptr<User>>		_starringUsers; // Users that starred this release
};

} // namespace Database


