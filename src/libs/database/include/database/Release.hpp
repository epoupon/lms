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
#include <vector>

#include <Wt/WDateTime.h>
#include <Wt/Dbo/Dbo.h>

#include "database/Types.hpp"
#include "utils/UUID.hpp"

namespace Database
{

class Artist;
class Cluster;
class ClusterType;
class Release;
class Session;
class Track;
class User;

class Release : public Object<Release, ReleaseId>
{
	public:
		Release() = default;
		Release(const std::string& name, const std::optional<UUID>& MBID = {});

		// Accessors
		static std::size_t		getCount(Session& session);
		static pointer			getByMBID(Session& session, const UUID& MBID);
		static std::vector<pointer>	getByName(Session& session, const std::string& name);
		static pointer			getById(Session& session, ReleaseId id);
		static std::vector<pointer>	getAllOrphans(Session& session); // no track related
		static std::vector<pointer>	getAll(Session& session, std::optional<Range> range = std::nullopt);
		static std::vector<ReleaseId>	getAllIds(Session& session);
		static std::vector<pointer>	getAllOrderedByArtist(Session& session, std::optional<std::size_t> offset = {}, std::optional<std::size_t> size = {});
		static std::vector<pointer>	getAllRandom(Session& session, const std::vector<ClusterId>& clusters, std::optional<std::size_t> size = {});
		static std::vector<ReleaseId>	getAllIdsRandom(Session& session, const std::vector<ClusterId>& clusters, std::optional<std::size_t> size = {});
		static std::vector<pointer>	getLastWritten(Session& session, std::optional<Wt::WDateTime> after, const std::vector<ClusterId>& clusters, std::optional<Range> range, bool& moreResults);
		static std::vector<pointer>	getByYear(Session& session, int yearFrom, int yearTo, std::optional<Range> range = std::nullopt);
		static std::vector<pointer> getStarred(Session& session, ObjectPtr<User> user, const std::vector<ClusterId>& clusters, std::optional<Range> range, bool& moreResults);

		static std::vector<pointer>	getByClusters(Session& session, const std::vector<ClusterId>& clusters);
		static std::vector<pointer>	getByFilter(Session& session,
							const std::vector<ClusterId>& clusters,		// if non empty, at least one release that belongs to these clusters
							const std::vector<std::string_view>& keywords,	// if non empty, name must match all of these keywords
							std::optional<Range> range,
							bool& moreExpected);
		static std::vector<ReleaseId>	getAllIdsWithClusters(Session& session, std::optional<std::size_t> limit = {});

		std::vector<ObjectPtr<Track>> getTracks(const std::vector<ClusterId>& clusters = {}) const;
		std::size_t					getTracksCount() const;
		ObjectPtr<Track>			getFirstTrack() const;

		// Get the cluster of the tracks that belong to this release
		// Each clusters are grouped by cluster type, sorted by the number of occurence (max to min)
		// size is the max number of cluster per cluster type
		std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(const std::vector<ObjectPtr<ClusterType>>& clusterTypes, std::size_t size) const;

		// Create
		static pointer	create(Session& session, const std::string& name, const std::optional<UUID>& MBID = {});

		// Utility functions
		std::optional<int>			getReleaseYear(bool originalDate = false) const;
		std::optional<std::string>	getCopyright() const;
		std::optional<std::string>	getCopyrightURL() const;

		// Accessors
		const std::string&		getName() const		{ return _name; }
		std::optional<UUID>		getMBID() const		{ return UUID::fromString(_MBID); }
		std::optional<std::size_t>	getTotalTrack() const;
		std::optional<std::size_t>	getTotalDisc() const;
		std::chrono::milliseconds	getDuration() const;
		Wt::WDateTime				getLastWritten() const;

		// Get the artists of this release
		std::vector<ObjectPtr<Artist> > getArtists(TrackArtistLinkType type = TrackArtistLinkType::Artist) const;
		std::vector<ObjectPtr<Artist> > getReleaseArtists() const { return getArtists(TrackArtistLinkType::ReleaseArtist); }
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


