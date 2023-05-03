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
#include <string>
#include <string_view>
#include <vector>

#include <Wt/WDateTime.h>
#include <Wt/Dbo/Dbo.h>

#include "services/database/ArtistId.hpp"
#include "services/database/ClusterId.hpp"
#include "services/database/Object.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"
#include "services/database/TrackId.hpp"
#include "utils/EnumSet.hpp"
#include "utils/UUID.hpp"

namespace Database
{

class Cluster;
class ClusterType;
class Release;
class Session;
class StarredArtist;
class Track;
class TrackArtistLink;
class User;

class Artist final : public Object<Artist, ArtistId>
{
	public:
		struct FindParameters
		{
			std::vector<ClusterId>				clusters;	// if non empty, at least one artist that belongs to these clusters
			std::vector<std::string_view>		keywords;	// if non empty, name must match all of these keywords (on either name field OR sort name field)
			std::optional<TrackArtistLinkType>	linkType;	// if set, only artists that have produced at least one track with this link type
			ArtistSortMethod					sortMethod {ArtistSortMethod::None};
			Range								range;
			Wt::WDateTime						writtenAfter;
			UserId								starringUser;	// only artists starred by this user
			std::optional<Scrobbler>			scrobbler;		// and for this scrobbler
			TrackId								track;		// artists involved in this track
			ReleaseId							release;	// artists involved in this release

			FindParameters& setClusters(const std::vector<ClusterId>& _clusters) { clusters = _clusters; return *this; }
			FindParameters& setKeywords(const std::vector<std::string_view>& _keywords) { keywords = _keywords; return *this; }
			FindParameters& setLinkType(std::optional<TrackArtistLinkType> _linkType) { linkType = _linkType; return *this; }
			FindParameters& setSortMethod(ArtistSortMethod _sortMethod) {sortMethod = _sortMethod; return *this; }
			FindParameters& setRange(Range _range) {range = _range; return *this; }
			FindParameters& setWrittenAfter(const Wt::WDateTime& _after) { writtenAfter = _after; return *this; }
			FindParameters& setStarringUser(UserId _user, Scrobbler _scrobbler) { starringUser = _user; scrobbler = _scrobbler; return *this; }
			FindParameters& setTrack(TrackId _track) { track = _track; return *this; }
			FindParameters& setRelease(ReleaseId _release) { release = _release; return *this; }
		};

		Artist() = default;

		// Accessors
		static std::size_t				getCount(Session& session);
		static pointer					find(Session& session, const UUID& MBID);
		static pointer					find(Session& session, ArtistId id);
		static std::vector<pointer>		find(Session& session, const std::string& name);		// exact match on name field
		static RangeResults<ArtistId>	find(Session& session, const FindParameters& parameters);
		static RangeResults<ArtistId>	findAllOrphans(Session& session, Range range); // No track related
		static bool						exists(Session& session, ArtistId id);

		// Accessors
		const std::string&	getName() const { return _name; }
		const std::string&	getSortName() const { return _sortName; }
		std::optional<UUID>	getMBID() const { return UUID::fromString(_MBID); }

		// No artistLinkTypes means get them all
		RangeResults<ArtistId>				findSimilarArtists(EnumSet<TrackArtistLinkType> artistLinkTypes = {}, Range range = {}) const;

		// Get the cluster of the tracks made by this artist
		// Each clusters are grouped by cluster type, sorted by the number of occurence
		// size is the max number of cluster per cluster type
		std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(std::vector<ObjectPtr<ClusterType>> clusterTypes, std::size_t size) const;

		void setName(std::string_view name)		{ _name  = name; }
		void setMBID(const std::optional<UUID>& mbid)	{ _MBID = mbid ? mbid->getAsString() : ""; }
		void setSortName(const std::string& sortName);

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name, "name");
				Wt::Dbo::field(a, _sortName, "sort_name");
				Wt::Dbo::field(a, _MBID, "mbid");

				Wt::Dbo::hasMany(a, _trackArtistLinks, Wt::Dbo::ManyToOne, "artist");
				Wt::Dbo::hasMany(a, _starredArtists, Wt::Dbo::ManyToMany, "user_starred_artists", "", Wt::Dbo::OnDeleteCascade);
			}

	private:
		static constexpr std::size_t _maxNameLength {128};

		friend class Session;
		// Create
		Artist(const std::string& name, const std::optional<UUID>& MBID = {});
		static pointer create(Session& session, const std::string& name, const std::optional<UUID>& UUID = {});

		std::string _name;
		std::string _sortName;
		std::string _MBID;	// Musicbrainz Identifier

		Wt::Dbo::collection<Wt::Dbo::ptr<TrackArtistLink>>	_trackArtistLinks;	// Tracks involving this artist
		Wt::Dbo::collection<Wt::Dbo::ptr<StarredArtist>>	_starredArtists; 	// starred entries for this artist
};

} // namespace Database

