/*
 * Copyright (C) 2016 Emeric Poupon
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

#include "Track.hpp"

#include "Classification.hpp"

namespace Database {

Feature::Feature(Wt::Dbo::ptr<Track> track, const std::string& type, const std::string& value)
: _type(type),
_value(value),
_track(track)
{
}

Feature::pointer
Feature::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, const std::string& type, const std::string& value)
{
	return session.add(new Feature(track, type, value));
}

std::vector<Feature::pointer>
Feature::getByTrack(Wt::Dbo::Session& session, Track::id_type trackId, const std::string& type)
{
	Wt::Dbo::collection<pointer> res = session.find<Feature>().where("track_id = ? AND type = ?").bind( trackId).bind(type);
	return std::vector<pointer>(res.begin(), res.end());
}

} // namespace Database

