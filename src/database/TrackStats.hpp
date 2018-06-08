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
#include <Wt/WDateTime.h>

#include <string>

#include "Types.hpp"

namespace Database {

class Artist;
class Release;
class Track;
class User;

class TrackStats
{
	public:
		using pointer = Wt::Dbo::ptr<TrackStats>;

		TrackStats() {}
		TrackStats(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<User> user);

		static std::vector<Wt::Dbo::ptr<Artist>> getMostPlayedArtists(Wt::Dbo::Session& session, Wt::Dbo::ptr<User>, int limit = 1);
		static std::vector<Wt::Dbo::ptr<Artist>> getLastPlayedArtists(Wt::Dbo::Session& session, Wt::Dbo::ptr<User>, int limit = 1);

		static std::vector<Wt::Dbo::ptr<Release>> getMostPlayedReleases(Wt::Dbo::Session& session, Wt::Dbo::ptr<User>, int limit = 1);
		static std::vector<Wt::Dbo::ptr<Release>> getLastPlayedReleases(Wt::Dbo::Session& session, Wt::Dbo::ptr<User>, int limit = 1);

		// Get utility (will create if does not exist)
		static pointer	get(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<User> user);

		void incPlayCount() { _playCount++; }
		void setLastPlayed(Wt::WDateTime lastPlayed) { _lastPlayed = lastPlayed; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a,	_playCount, "play_count");
			Wt::Dbo::field(a,	_lastPlayed, "last_played");
			Wt::Dbo::belongsTo(a,	_track, "track", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::belongsTo(a,	_user, "user", Wt::Dbo::OnDeleteCascade);
		}

	private:

		int _playCount = 0;
		Wt::WDateTime _lastPlayed;
		Wt::Dbo::ptr<Track> _track;
		Wt::Dbo::ptr<User> _user;
};


} // namespace Database

