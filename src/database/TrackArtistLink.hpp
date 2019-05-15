/*
 * Copyright (C) 2013-2016 Emeric Poupon
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

#include "Types.hpp"

namespace Database {

class Artist;
class Track;

class TrackArtistLink
{
	public:
		enum class Type
		{
			Artist,	// regular artist
			Arranger,
			Composer,
			Conductor,
			Lyricist,
			Mixer,
			Performer,
			Producer,
			ReleaseArtist,
			Remixer,
			Writer,
		};

		using pointer = Wt::Dbo::ptr<TrackArtistLink>;

		TrackArtistLink() = default;
		TrackArtistLink(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Artist> artist, Type type);

		static pointer create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<Artist> artist,Type type);

		Wt::Dbo::ptr<Track>	getTrack() const { return _track; }
		Wt::Dbo::ptr<Artist>	getArtist() const { return _artist; }
		Type			getType() const { return _type; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _type, "type");
			Wt::Dbo::field(a, _type, "name");

			Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
		}

	private:

		Type		_type;
		std::string	_name;

		Wt::Dbo::ptr<Track> _track;
		Wt::Dbo::ptr<Artist> _artist;
};

}


