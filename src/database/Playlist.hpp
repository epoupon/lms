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

class PlaylistEntry;
class User;
class Track;
class Cluster;

class Playlist : public Wt::Dbo::Dbo<Playlist>
{
	public:
		using pointer = Wt::Dbo::ptr<Playlist>;

		Playlist();
		Playlist(std::string name, bool isPublic, Wt::Dbo::ptr<User> user);

		// Search utility
		static pointer	get(Wt::Dbo::Session& session, std::string name, Wt::Dbo::ptr<User> user);
		static std::vector<pointer> getAll(Wt::Dbo::Session& session, Wt::Dbo::ptr<User> user);

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, std::string name, bool isPublic, Wt::Dbo::ptr<User> user);

		// Accessors
		std::string	getName() const { return _name; }
		bool		isPublic() const { return _isPublic; }

		// Modifiers
		void clear() { _entries.clear(); }

		// Get tracks, ordered by position
		std::size_t getCount() const;
		Wt::Dbo::ptr<PlaylistEntry> getEntry(std::size_t pos) const;
		std::vector<Wt::Dbo::ptr<PlaylistEntry>> getEntries(int offset, int size, bool& moreResults) const;
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
			Wt::Dbo::hasMany(a, _entries, Wt::Dbo::ManyToOne, "playlist");
		}

	private:

		std::string		_name;
		bool			_isPublic;
		Wt::Dbo::ptr<User>	_user;
		Wt::Dbo::collection< Wt::Dbo::ptr<PlaylistEntry> > _entries;

};

class PlaylistEntry
{
	public:

		using pointer = Wt::Dbo::ptr<PlaylistEntry>;

		PlaylistEntry();
		PlaylistEntry(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Playlist> playlist);

		static pointer getById(Wt::Dbo::Session& session, IdType id);

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Playlist> playlist);

		// Accessors
		Wt::Dbo::ptr<Track>	getTrack() const { return _track; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::belongsTo(a,	_track, "track", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::belongsTo(a,	_playlist, "playlist", Wt::Dbo::OnDeleteCascade);
		}

	private:

		Wt::Dbo::ptr<Track>	_track;
		Wt::Dbo::ptr<Playlist>	_playlist;
};

} // namespace Database

