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

#include "logger/Logger.hpp"

#include "database/Setting.hpp"
#include "feature/FeatureStore.hpp"

#include "DatabaseHighLevelCluster.hpp"

namespace Database {

static Cluster::pointer getCluster(std::string type, std::string value)
{
	Cluster::pointer cluster = ( Cluster::get(UpdaterDboSession(), type, value) );
	if (!cluster)
		cluster = Cluster::create(UpdaterDboSession(), type, value);

	return cluster;
}

static std::list<std::string> getClustersFromFeature(Feature::Type& feature, double minProb)
{
	struct HighLevelNodeDesc
	{
		std::string	node;
		std::map<std::string, std::string>	valueMapping;
	};

	static const std::vector<HighLevelNodeDesc> nodes =
	{
		{
			"highlevel.danceability",
			{
				{"not_danceable",	"Not danceable"},
				{"danceable",		"Danceable"},
			},
		},
		{
			"highlevel.gender",
			{
				{"male",		"Male"},
				{"female",		"Female"},
			},
		},
		{
			"highlevel.mood_acoustic",
			{
				{"not_acoustic",	"Not acoustic"},
				{"acoustic",		"Acoustic"},
			},
		},
		{
			"highlevel.mood_happy",
			{
				{"not_happy",		"Not happy"},
				{"happy",		"Happy"},
			},
		},
		{
			"highlevel.mood_aggressive",
			{
				{"not_aggressive",	"Not aggressive"},
				{"aggressive",		"Aggressive"},
			},
		},
		{
			"highlevel.mood_electronic",
			{
				{"not_electronic",	"Not electronic"},
				{"electronic",		"Electronic"},
			},
		},
		{
			"highlevel.mood_party",
			{
				{"not_party",		"Not party"},
				{"party",		"Party"},
			},
		},
		{
			"highlevel.mood_relaxed",
			{
				{"not_relaxed",		"Not relaxed"},
				{"relaxed",		"Relaxed"},
			},
		},
		{
			"highlevel.mood_sad",
			{
				{"not_sad",		"Not sad"},
				{"sad",			"Sad"},
			},
		},
		{
			"highlevel.timbre",
			{
				{"bright",		"Bright"},
				{"dark",		"Dark"},
			},
		},
		{
			"highlevel.tonal_atonal",
			{
				{"atonal",		"Atonal"},
				{"tonal",		"Tonal"},
			},
		},
		{
			"highlevel.voice_instrumental",
			{
				{"instrumental",	"Instrumental"},
				{"voice",		"Voice"},
			},
		},
	};


	// Extract info and build clusters
	std::list<std::string> newClusterNames;

	for (auto node : nodes)
	{
		auto value = feature.get_child_optional(node.node + ".value");
		auto probability = feature.get_child_optional(node.node + ".probability");

		if (!probability || !value)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Missing " << node.node;
			continue;
		}

		if (std::stod(probability->data()) < 0.90)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Probability too low for " << node.node << "(" << std::stod(probability->data()) << ")";
			continue;
		}

		if (node.valueMapping[value->data()] == "")
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Unknown value '" << value->data() << "'";
			continue;
		}

		newClusterNames.push_back(node.valueMapping[value->data()]);
	}

	return newClusterNames;
}

void
HighLevelCluster::processDatabaseUpdate(Updater::Stats stats)
{
	bool createTags = Setting::getBool(UpdaterDboSession(), "tags_highlevel_acousticbrainz", false);
	double minProb = Setting::getInt(UpdaterDboSession(), "tags_highlevel_acousticbrainz_min_probability", false) / 100.;

	LMS_LOG(DBUPDATER, INFO) << "Creating high level based clusters...";

	std::vector<Track::id_type> trackIds = Track::getAllIds(UpdaterDboSession());

	LMS_LOG(DBUPDATER, DEBUG) << "Got " << trackIds.size() << " tracks";
	for (auto trackId : trackIds)
	{
		if (UpdaterQuitRequested())
			return;

		// Get current cluster names
		std::list<std::string> newClusterNames;
		if (createTags)
		{
			Feature::Type feature;
			if (!Feature::Store::instance().get(UpdaterDboSession(), trackId, "high_level", feature))
				continue;

			newClusterNames = getClustersFromFeature(feature, minProb);
		}

		Wt::Dbo::Transaction transaction(UpdaterDboSession());

		auto track = Track::getById(UpdaterDboSession(), trackId);

		auto clusters = track->getClusters();
		for (auto cluster : clusters)
		{
			// Check if removed
			if (cluster->getType() != "high_level")
				continue;

			auto it = std::find(newClusterNames.begin(), newClusterNames.end(), cluster->getName());
			if (it == newClusterNames.end())
				cluster.remove();
			else
				newClusterNames.erase(it);

		}

		// Add previsouly missing clusters
		for (auto newName : newClusterNames)
		{
			auto cluster = getCluster("high_level", newName);
			cluster.modify()->addTrack(track);
		}
	}

	LMS_LOG(DBUPDATER, INFO) << "High level based clusters processed";
}


} // namespace Database
