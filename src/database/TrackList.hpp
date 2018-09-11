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

#include <Wt/Dbo/Dbo.h>

#include <string>

#include "Types.hpp"

namespace Database {

class Artist;
class Release;
class User;
class Track;
class TrackListEntry;
class Cluster;

class TrackList : public Wt::Dbo::Dbo<TrackList>
{
	public:
		using pointer = Wt::Dbo::ptr<TrackList>;

		TrackList();
		TrackList(std::string name, bool isPublic, Wt::Dbo::ptr<User> user);

		// Stats utility
		std::vector<Wt::Dbo::ptr<Artist>> getTopArtists(int limit = 1) const;
		std::vector<Wt::Dbo::ptr<Release>> getTopReleases(int limit = 1) const;
		std::vector<Wt::Dbo::ptr<Track>> getTopTracks(int limit = 1) const;

		// Search utility
		static pointer	get(Wt::Dbo::Session& session, std::string name, Wt::Dbo::ptr<User> user);
		static pointer	getById(Wt::Dbo::Session& session, IdType tracklistId);
		static std::vector<pointer> getAll(Wt::Dbo::Session& session, Wt::Dbo::ptr<User> user);

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, std::string name, bool isPublic, Wt::Dbo::ptr<User> user);

		// Accessors
		std::string	getName() const { return _name; }
		bool		isPublic() const { return _isPublic; }

		// Modifiers
		Wt::Dbo::ptr<TrackListEntry> add(IdType trackId);
		void clear() { _entries.clear(); }
		void shuffle();

		// Get tracks, ordered by position
		std::size_t getCount() const;
		Wt::Dbo::ptr<TrackListEntry> getEntry(std::size_t pos) const;
		std::vector<Wt::Dbo::ptr<TrackListEntry>> getEntries(int offset = -1, int size = -1) const;
		std::vector<Wt::Dbo::ptr<TrackListEntry>> getEntriesReverse(int offset = -1, int size = -1) const;

		std::vector<IdType> getTrackIds() const;

		// Get clusters, order by occurence
		std::vector<Wt::Dbo::ptr<Cluster>> getClusters() const;

		bool hasTrack(IdType trackId) const;

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a,	_name,		"name");
			Wt::Dbo::field(a,	_isPublic,	"public");
			Wt::Dbo::belongsTo(a,	_user,		"user", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::hasMany(a, _entries, Wt::Dbo::ManyToOne, "tracklist");
		}

	private:

		std::string		_name;
		bool			_isPublic;
		Wt::Dbo::ptr<User>	_user;
		Wt::Dbo::collection< Wt::Dbo::ptr<TrackListEntry> > _entries;

};

class TrackListEntry : public Wt::Dbo::Dbo<TrackListEntry>
{
	public:

		using pointer = Wt::Dbo::ptr<TrackListEntry>;

		TrackListEntry();
		TrackListEntry(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist);

		static pointer getById(Wt::Dbo::Session& session, IdType id);

		// Create utility
		static pointer create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist);

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

