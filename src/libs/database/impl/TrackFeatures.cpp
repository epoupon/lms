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

#include "database/TrackFeatures.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "utils/Logger.hpp"

namespace Database {

TrackFeatures::TrackFeatures(ObjectPtr<Track> track, const std::string& jsonEncodedFeatures)
: _data {jsonEncodedFeatures},
_track {getDboPtr(track)}
{
}

TrackFeatures::pointer
TrackFeatures::create(Session& session, ObjectPtr<Track> track, const std::string& jsonEncodedFeatures)
{
	session.checkUniqueLocked();
	return session.getDboSession().add(std::make_unique<TrackFeatures>(track, jsonEncodedFeatures));
}

FeatureValues
TrackFeatures::getFeatureValues(const FeatureName& featureNode) const
{
	FeatureValuesMap featuresValuesMap {getFeatureValuesMap({featureNode})};
	return std::move(featuresValuesMap[featureNode]);
}

FeatureValuesMap
TrackFeatures::getFeatureValuesMap(const std::unordered_set<FeatureName>& featureNames) const
{
	try
	{
		std::istringstream iss {_data};
		boost::property_tree::ptree root;

		boost::property_tree::read_json(iss, root);

		FeatureValuesMap res;
		for (const FeatureName& featureName : featureNames)
		{
			FeatureValues& featureValues {res[featureName]};

			auto node {root.get_child(featureName)};

			bool hasChildren = false;
			for (const auto& child : node.get_child(""))
			{
				hasChildren = true;
				featureValues.push_back(child.second.get_value<double>());
			}

			if (!hasChildren)
				featureValues.push_back(node.get_value<double>());
		}

		return res;
	}
	catch (boost::property_tree::ptree_error& error)
	{
		LMS_LOG(DB, ERROR) << "Track " << _track.id() << ": ptree exception: " << error.what();
		return {};
	}
}

} // namespace Database
