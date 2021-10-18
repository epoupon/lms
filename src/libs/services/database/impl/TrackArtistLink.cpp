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

#include "database/TrackArtistLink.hpp"

#include "database/Artist.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "Traits.hpp"

namespace Database {

TrackArtistLink::TrackArtistLink(ObjectPtr<Track> track, ObjectPtr<Artist> artist, TrackArtistLinkType type)
: _type {type},
_track {getDboPtr(track)},
_artist {getDboPtr(artist)}
{
}

TrackArtistLink::pointer
TrackArtistLink::create(Session& session, ObjectPtr<Track> track, ObjectPtr<Artist> artist, TrackArtistLinkType type)
{
	session.checkUniqueLocked();

	TrackArtistLink::pointer res {session.getDboSession().add(std::make_unique<TrackArtistLink>(track, artist, type))};
	session.getDboSession().flush();

	return res;
}

EnumSet<TrackArtistLinkType>
TrackArtistLink::getUsedTypes(Session& session)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().query<TrackArtistLinkType>("SELECT DISTINCT type from track_artist_link").resultList()};

	return EnumSet<TrackArtistLinkType>(std::begin(res), std::end(res));
}

}

