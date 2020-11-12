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
#include <set>

#include <Wt/Dbo/Dbo.h>

#include "Types.hpp"

namespace Database {

class Artist;
class Cluster;
class Release;
class Session;
class Track;
class TrackListEntry;
class User;

class TrackList : public Wt::Dbo::Dbo<TrackList>
{
	public:
		using pointer = Wt::Dbo::ptr<TrackList>;

		enum class Type
		{
			Playlist,  // user controlled playlists
			Internal,  // current playqueue, history
		};

		TrackList() = default;
		TrackList(const std::string& name, Type type, bool isPublic, Wt::Dbo::ptr<User> user);

		// Stats utility
		std::vector<Wt::Dbo::ptr<Artist>> getTopArtists(const std::set<IdType>& clusterIds, std::optional<TrackArtistLinkType> linkType, std::optional<Range> range, bool& moreResults) const;
		std::vector<Wt::Dbo::ptr<Release>> getTopReleases(const std::set<IdType>& clusterIds, std::optional<Range> range, bool& moreResults) const;
		std::vector<Wt::Dbo::ptr<Track>> getTopTracks(const std::set<IdType>& clusterIds, std::optional<Range> range, bool& moreResults) const;

		// Search utility
		static pointer	get(Session& session, const std::string& name, Type type, Wt::Dbo::ptr<User> user);
		static pointer	getById(Session& session, IdType tracklistId);
		static std::vector<pointer> getAll(Session& session);
		static std::vector<pointer> getAll(Session& session, Wt::Dbo::ptr<User> user);
		static std::vector<pointer> getAll(Session& session, Wt::Dbo::ptr<User> user, Type type);

		// Create utility
		static pointer	create(Session& session, const std::string& name, Type type, bool isPublic, Wt::Dbo::ptr<User> user);

		// Accessors
		std::string	getName() const { return _name; }
		bool		isPublic() const { return _isPublic; }
		Type		getType() const { return _type; }
		Wt::Dbo::ptr<User> getUser() const { return _user; }

		// Modifiers
		void		setName(const std::string& name) { _name = name; }
		void		setIsPublic(bool isPublic) { _isPublic = isPublic; }
		void		clear() { _entries.clear(); }

		// Get tracks, ordered by position
		bool isEmpty() const;
		std::size_t getCount() const;
		Wt::Dbo::ptr<TrackListEntry> getEntry(std::size_t pos) const;
		std::vector<Wt::Dbo::ptr<TrackListEntry>> getEntries(std::optional<std::size_t> offset = {}, std::optional<std::size_t> size = {}) const;
		std::vector<Wt::Dbo::ptr<TrackListEntry>> getEntriesReverse(std::optional<std::size_t>  offset = {}, std::optional<std::size_t> size = {}) const;

		std::vector<Wt::Dbo::ptr<Artist>> getArtistsReverse(const std::set<IdType>& clusterIds, std::optional<TrackArtistLinkType> linkType, std::optional<Range> range, bool& moreResults) const;
		std::vector<Wt::Dbo::ptr<Release>> getReleasesReverse(const std::set<IdType>& clusterIds, std::optional<Range> range, bool& moreResults) const;
		std::vector<Wt::Dbo::ptr<Track>> getTracksReverse(const std::set<IdType>& clusterIds, std::optional<Range> range, bool& moreResults) const;

		std::vector<IdType> getTrackIds() const;

		std::chrono::milliseconds getDuration() const;

		// Get clusters, order by occurence
		std::vector<Wt::Dbo::ptr<Cluster>> getClusters() const;

		bool hasTrack(IdType trackId) const;

		// Ordered from most clusters in common
		std::vector<Wt::Dbo::ptr<Track>> getSimilarTracks(std::optional<std::size_t> offset = {}, std::optional<std::size_t> size = {}) const;

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a,	_name,		"name");
			Wt::Dbo::field(a,	_type,		"type");
			Wt::Dbo::field(a,	_isPublic,	"public");

			Wt::Dbo::belongsTo(a,	_user,		"user", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::hasMany(a, _entries, Wt::Dbo::ManyToOne, "tracklist");
		}

	private:

		std::string		_name;
		Type			_type {Type::Playlist};
		bool			_isPublic {false};

		Wt::Dbo::ptr<User>	_user;
		Wt::Dbo::collection< Wt::Dbo::ptr<TrackListEntry> > _entries;

};

class TrackListEntry : public Wt::Dbo::Dbo<TrackListEntry>
{
	public:

		using pointer = Wt::Dbo::ptr<TrackListEntry>;

		TrackListEntry() = default;
		TrackListEntry(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist);

		static pointer getById(Session& session, IdType id);

		// Create utility
		static pointer create(Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist);

		// Accessors
		Wt::Dbo::ptr<Track>	getTrack() const { return _track; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::belongsTo(a,	_track, "track", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::belongsTo(a,	_tracklist, "tracklist", Wt::Dbo::OnDeleteCascade);
		}

	private:

		Wt::Dbo::ptr<Track>	_track;
		Wt::Dbo::ptr<TrackList>	_tracklist;
};

} // namespace Database

