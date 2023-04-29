/*
 * Copyright (C) 2014 Emeric Poupon
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

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "services/database/ClusterId.hpp"
#include "services/database/Object.hpp"
#include "services/database/TrackId.hpp"
#include "services/database/TrackListId.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"

namespace Database {

class Artist;
class Cluster;
class ClusterType;
class Release;
class Session;
class Track;
class TrackListEntry;
class User;

class TrackList final : public Object<TrackList, TrackListId>
{
	public:
		TrackList() = default;

		// Stats utility
		std::vector<ObjectPtr<Artist>>	getTopArtists(const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, std::optional<Range> range, bool& moreResults) const;
		std::vector<ObjectPtr<Release>>	getTopReleases(const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults) const;
		std::vector<ObjectPtr<Track>>	getTopTracks(const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults) const;

		// Search utility
		struct FindParameters
		{
			std::vector<ClusterId>			clusters;	// if non empty, tracklists that have tracks that belong to these clusters
			Range                           range;
			std::optional<TrackListType>	type;
			UserId							user;		// only tracklists owned by this user
			TrackListSortMethod				sortMethod {TrackListSortMethod::None};

			FindParameters& setClusters(const std::vector<ClusterId>& _clusters) { clusters = _clusters; return *this; }
			FindParameters& setRange(Range _range) { range = _range; return *this; }
			FindParameters& setType(TrackListType _type) { type = _type; return *this; }
			FindParameters& setUser(UserId _user) { user = _user; return *this; }
			FindParameters& setSortMethod(TrackListSortMethod _sortMethod) {sortMethod = _sortMethod; return *this; }
		};
		static std::size_t					getCount(Session& session);
		static pointer						find(Session& session, std::string_view name, TrackListType type, UserId userId);
		static pointer						find(Session& session, TrackListId tracklistId);
		static RangeResults<TrackListId>	find(Session& session, const FindParameters& params);

		// Accessors
		std::string_view	getName() const { return _name; }
		bool				isPublic() const { return _isPublic; }
		TrackListType		getType() const { return _type; }
		ObjectPtr<User>		getUser() const { return _user; }

		// Modifiers
		void		setName(const std::string& name) { _name = name; }
		void		setIsPublic(bool isPublic) { _isPublic = isPublic; }
		void		clear() { _entries.clear(); }

		// Get tracks, ordered by position
		bool										isEmpty() const;
		std::size_t									getCount() const;
		ObjectPtr<TrackListEntry>					getEntry(std::size_t pos) const;
		std::vector<ObjectPtr<TrackListEntry>>		getEntries(std::optional<Range> range = {}) const;
		ObjectPtr<TrackListEntry>					getEntryByTrackAndDateTime(ObjectPtr<Track> track, const Wt::WDateTime& dateTime) const;

		std::vector<ObjectPtr<Artist>>				getArtists(const std::vector<ClusterId>& clusters, std::optional<TrackArtistLinkType> linkType, ArtistSortMethod sortMethod, std::optional<Range> range, bool& moreResults) const;
		std::vector<ObjectPtr<Release>>				getReleases(const std::vector<ClusterId>& clusters, std::optional<Range> range, bool& moreResults) const;
		std::vector<ObjectPtr<Track>>				getTracks(const std::vector<ClusterId>& clusters, std::optional<Range> range, bool& moreResults) const;

		// Sorted by date time
		std::vector<ObjectPtr<Artist>>				getArtistsOrderedByRecentFirst(const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, std::optional<Range> range, bool& moreResults) const;
		std::vector<ObjectPtr<Release>>				getReleasesOrderedByRecentFirst(const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults) const;
		std::vector<ObjectPtr<Track>>				getTracksOrderedByRecentFirst(const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults) const;

		std::vector<TrackId>						getTrackIds() const;
		std::chrono::milliseconds					getDuration() const;

		void										setLastModifiedDateTime(const Wt::WDateTime& dateTime);

		// Get clusters, order by occurence
		std::vector<ObjectPtr<Cluster>> getClusters() const;
		std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(const std::vector<ObjectPtr<ClusterType>>& clusterTypes, std::size_t size) const;

		bool hasTrack(TrackId trackId) const;

		// Ordered from most clusters in common
		std::vector<ObjectPtr<Track>> getSimilarTracks(std::optional<std::size_t> offset = {}, std::optional<std::size_t> size = {}) const;

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a,	_name,				"name");
			Wt::Dbo::field(a,	_type,				"type");
			Wt::Dbo::field(a,	_isPublic,			"public");
			Wt::Dbo::field(a,	_creationDateTime,	"creation_date_time");
			Wt::Dbo::field(a,	_lastModifiedDateTime,	"last_modified_date_time");

			Wt::Dbo::belongsTo(a,	_user,		"user", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::hasMany(a, _entries, Wt::Dbo::ManyToOne, "tracklist");
		}

	private:
		friend class Session;
		TrackList(std::string_view name, TrackListType type, bool isPublic, ObjectPtr<User> user);
		static pointer create(Session& session, std::string_view name, TrackListType type, bool isPublic, ObjectPtr<User> user);

		std::string		_name;
		TrackListType	_type {TrackListType::Playlist};
		bool			_isPublic {false};
		Wt::WDateTime	_creationDateTime;
		Wt::WDateTime	_lastModifiedDateTime;

		Wt::Dbo::ptr<User>	_user;
		Wt::Dbo::collection<Wt::Dbo::ptr<TrackListEntry>> _entries;
};

class TrackListEntry final : public Object<TrackListEntry, TrackListEntryId>
{
	public:
		TrackListEntry() = default;

		bool hasOnPostCreated() const override { return true; }
		void onPostCreated() override;

		bool hasOnPreRemove() const override { return true; }
		void onPreRemove() override;

		// find utility
		static pointer getById(Session& session, TrackListEntryId id);

		// Accessors
		ObjectPtr<Track>	getTrack() const { return _track; }
		const Wt::WDateTime& getDateTime() const { return _dateTime; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a,		_dateTime,	"date_time");

			Wt::Dbo::belongsTo(a,	_track,		"track",		Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::belongsTo(a,	_tracklist,	"tracklist",	Wt::Dbo::OnDeleteCascade);
		}

	private:
		friend class Session;
		TrackListEntry(ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime);
		TrackListEntry(ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist);
		static pointer create(Session& session, ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime = {});

		Wt::WDateTime			_dateTime;		// optional date time
		Wt::Dbo::ptr<Track>		_track;
		Wt::Dbo::ptr<TrackList>	_tracklist;
};

} // namespace Database

