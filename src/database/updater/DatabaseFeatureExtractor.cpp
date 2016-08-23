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

#include "database/Setting.hpp"

#include "feature/FeatureExtractor.hpp"
#include "feature/FeatureStore.hpp"

#include "utils/Logger.hpp"

#include "DatabaseFeatureExtractor.hpp"

namespace Database {

static std::string getMBID(Track::id_type trackId)
{
	Wt::Dbo::Transaction transaction(UpdaterDboSession());
	Track::pointer track = Track::getById(UpdaterDboSession(), trackId);
	return track->getMBID();
}

void
FeatureExtractor::handleFilesUpdated(void)
{
	bool fetchHighLevel = Setting::getBool(UpdaterDboSession(), "tags_highlevel_acousticbrainz");
	bool fetchLowLevel = Setting::getBool(UpdaterDboSession(), "tags_similarity_acousticbrainz");

	if (!fetchHighLevel && !fetchLowLevel)
	{
		LMS_LOG(DBUPDATER, INFO) << "No need to extract features";
		return;
	}

	LMS_LOG(DBUPDATER, INFO) << "Processing tracks in order to extract features...";
	std::vector<Track::id_type> trackIds = Track::getAllIds(UpdaterDboSession());
	for (auto trackId : trackIds)
	{
		if (UpdaterQuitRequested())
			return;

		std::string mbid = getMBID(trackId);
		if (mbid.empty())
		{
			LMS_LOG(DBUPDATER, DEBUG) << "No MBID for track " << trackId << ", skipping";
			continue;
		}

		if (fetchLowLevel && !Feature::Store::instance().exists(UpdaterDboSession(), trackId, "low_level"))
		{
			boost::property_tree::ptree feature;
			if (::Feature::Extractor::getLowLevel(feature, mbid))
				Feature::Store::instance().set(UpdaterDboSession(), trackId, "low_level", feature);
		}

		if (fetchHighLevel && !Feature::Store::instance().exists(UpdaterDboSession(), trackId, "high_level"))
		{
			boost::property_tree::ptree feature;
			if (::Feature::Extractor::getHighLevel(feature, mbid))
				Feature::Store::instance().set(UpdaterDboSession(), trackId, "high_level", feature);
		}
	}

	LMS_LOG(DBUPDATER, INFO) << "Features have been extracted";
}

} // namespace Database

