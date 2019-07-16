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

class Session;
class Track;

class TrackFeatures : public Wt::Dbo::Dbo<TrackFeatures>
{
	public:

		using pointer = Wt::Dbo::ptr<TrackFeatures>;

		TrackFeatures() = default;
		TrackFeatures(Wt::Dbo::ptr<Track> track, const std::string& jsonEncodedFeatures);

		// Create utility
		static pointer create(Session& session, Wt::Dbo::ptr<Track> track, const std::string& jsonEncodedFeatures);

		std::vector<double> getFeatures(const std::string& featureNode) const;
		bool getFeatures(std::map<std::string /*featureNode*/, std::vector<double> /*values*/>& featureNodes) const;

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _data,	"data");
			Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
		}

	private:

		std::string _data;
		Wt::Dbo::ptr<Track> _track;
};


} // namespace database


