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

#pragma once

#include <string>

#include <Wt/Dbo/Dbo.h>

#include "Types.hpp"

namespace Database {

class Track;
class SimilaritySettings;

class TrackFeatureType : public Wt::Dbo::Dbo<TrackFeatureType>
{
	public:
		using pointer = Wt::Dbo::ptr<TrackFeatureType>;

		TrackFeatureType() = default;
		TrackFeatureType(std::string name);

		// Find utility
		static pointer getByName(Wt::Dbo::Session& session, std::string name);

		// Create utility
		static pointer create(Wt::Dbo::Session& session, std::string name);

		// Accessors
		const std::string& getName() const { return _name; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _name,	"name");
			Wt::Dbo::belongsTo(a, _similaritySettings, "similarity_settings", Wt::Dbo::OnDeleteCascade);
		}

	private:

		std::string _name;

		Wt::Dbo::ptr<SimilaritySettings> _similaritySettings;
};

class TrackFeature : public Wt::Dbo::Dbo<TrackFeature>
{
	public:

		using pointer = Wt::Dbo::ptr<TrackFeature>;

		TrackFeature() = default;
		TrackFeature(Wt::Dbo::ptr<TrackFeatureType> type, Wt::Dbo::ptr<Track> track, double value);

		// Create utility
		static pointer create(Wt::Dbo::Session& session, Wt::Dbo::ptr<TrackFeatureType> type, Wt::Dbo::ptr<Track> track, double value);

		Wt::Dbo::ptr<TrackFeatureType> getType() const { return _type; }
		double getValue() const { return _value; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _value,	"value");
			Wt::Dbo::belongsTo(a, _type, "type", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
		}

	private:

		Wt::Dbo::ptr<TrackFeatureType> _type;
		Wt::Dbo::ptr<Track> _track;
		double	_value = 0.;
};


} // namespace database


