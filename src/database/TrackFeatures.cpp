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

#include "TrackFeatures.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "utils/Logger.hpp"
#include "Track.hpp"

namespace Database {

TrackFeatures::TrackFeatures(Wt::Dbo::ptr<Track> track, const std::string& jsonEncodedFeatures)
: _data(jsonEncodedFeatures),
_track(track)
{
}

TrackFeatures::pointer
TrackFeatures::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, const std::string& jsonEncodedFeatures)
{
	return session.add(std::make_unique<TrackFeatures>(track, jsonEncodedFeatures));
}

std::vector<double>
TrackFeatures::getFeatures(const std::string& featureNode) const
{
	std::vector<double> res;

	std::map<std::string, std::vector<double>> features = { {featureNode, {}} };
	if (!getFeatures( features ))
		return res;

	res = std::move(features[featureNode]);

	return res;
}

bool
TrackFeatures::getFeatures(std::map<std::string /*name*/, std::vector<double> /*values*/>& features) const
{
	try
	{
		boost::property_tree::ptree root;

		std::istringstream iss(_data);
		boost::property_tree::read_json(iss, root);

		for (auto& featureNode : features)
		{
			auto node = root.get_child(featureNode.first);

			bool hasChildren = false;
			for (const auto& child : node.get_child(""))
			{
				hasChildren = true;
				featureNode.second.push_back(child.second.get_value<double>());
			}

			if (!hasChildren)
			{
				featureNode.second.push_back(node.get_value<double>());
			}
		}

		return true;
	}
	catch (boost::property_tree::ptree_error& error)
	{
		LMS_LOG(SIMILARITY, ERROR) << "Track " << _track.id() << ": ptree exception: " << error.what();
		return false;
	}
}

} // namespace Database
