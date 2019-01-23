/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "TrackFeature.hpp"

#include "SimilaritySettings.hpp"
#include "Track.hpp"

namespace Database {

TrackFeatureType::TrackFeatureType(std::string name)
	: _name(name)
{
}

TrackFeatureType::pointer
TrackFeatureType::getByName(Wt::Dbo::Session& session, std::string name)
{
	return session.find<TrackFeatureType>().where("name = ?").bind(name);
}

TrackFeatureType::pointer
TrackFeatureType::create(Wt::Dbo::Session& session, std::string name)
{
	return session.add(std::make_unique<TrackFeatureType>(name));
}


TrackFeature::TrackFeature(Wt::Dbo::ptr<TrackFeatureType> type, Wt::Dbo::ptr<Track> track, double value)
: _type(type),
_track(track),
_value(value)
{
}

TrackFeature::pointer
TrackFeature::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<TrackFeatureType> type, Wt::Dbo::ptr<Track> track, double value)
{
	return session.add(std::make_unique<TrackFeature>(type, track, value));
}

} // namespace Database
