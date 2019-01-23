/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "SimilaritySOMScannerAddon.hpp"

#include <cmath>

#include "database/Track.hpp"
#include "database/SimilaritySettings.hpp"
#include "database/TrackFeature.hpp"
#include "utils/Logger.hpp"

#include "AcousticBrainzUtils.hpp"
#include "DataNormalizer.hpp"
#include "Network.hpp"


namespace Similarity {

namespace {

struct TrackInfo
{
	Database::IdType id;
	std::string mbid;
};

std::vector<TrackInfo>
getTracksWithMBIDAndMissingFeatures(Wt::Dbo::Session& session)
{
	std::vector<TrackInfo> res;

	Wt::Dbo::Transaction transaction(session);

	auto tracks = Database::Track::getAllWithMBIDAndMissingFeatures(session);
	for (auto track : tracks)
		res.push_back({track.id(), track->getMBID()});

	return res;
}

std::vector<Database::TrackFeatureType::pointer>
getTrackFeatureTypes(Wt::Dbo::Session& session, const std::set<std::string>& featureNames)
{
	std::vector<Database::TrackFeatureType::pointer> res;

	for (const auto& featureName : featureNames)
	{
		auto trackFeatureType = Database::TrackFeatureType::getByName(session, featureName);
		if (!trackFeatureType)
		{
			LMS_LOG(DBUPDATER, ERROR) << "Missing feature type '" << featureName << "'";
			res.clear();
			return res;
		}

		res.push_back(trackFeatureType);
	}

	return res;
}


bool
extractFeatures(const Database::Track::pointer& track, const std::vector<Database::TrackFeatureType::pointer>& trackFeatureTypes, std::vector<double>& features)
{
	features.reserve(trackFeatureTypes.size());

	for (const auto& trackFeatureType : trackFeatureTypes)
	{
		auto feature = track->getTrackFeature(trackFeatureType);
		if (!feature)
		{
			LMS_LOG(DBUPDATER, ERROR) << "Missing feature " << trackFeatureType->getName() << " for track '" << track->getPath().string() << "'";
			return false;
		}

		features.emplace_back(feature->getValue());
	}

	return true;
}


} // namespace

SOMScannerAddon::SOMScannerAddon(Wt::Dbo::SqlConnectionPool& connectionPool)
: _db(connectionPool)
{
	refreshSettings();
	clusterize();
}

std::shared_ptr<Similarity::SOMSearcher>
SOMScannerAddon::getSearcher()
{
	return std::atomic_load(&_finder);
}

void
SOMScannerAddon::trackUpdated(Database::IdType trackId)
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	auto track = Database::Track::getById(_db.getSession(), trackId);
	if (!track)
		return;

	track.modify()->eraseFeatures();
}

void
SOMScannerAddon::preScanComplete()
{
	auto tracksInfo = getTracksWithMBIDAndMissingFeatures(_db.getSession());
	for (const auto& trackInfo : tracksInfo)
		fetchFeatures(trackInfo.id, trackInfo.mbid);

	LMS_LOG(DBUPDATER, INFO) << "Clustering tracks...";
	clusterize();
	LMS_LOG(DBUPDATER, INFO) << "Clusterization complete!";
}

void
SOMScannerAddon::clusterize()
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	auto trackFeatureTypes = getTrackFeatureTypes(_db.getSession(), _featuresName);

	LMS_LOG(DBUPDATER, DEBUG) << "Getting feature types DONE...";

	LMS_LOG(DBUPDATER, DEBUG) << "Getting Tracks with features...";
	auto tracks = Database::Track::getAllWithFeatures(_db.getSession());
	LMS_LOG(DBUPDATER, DEBUG) << "Getting Tracks with features DONE";

	std::vector<SOM::InputVector> samples;
	std::vector<Database::IdType> tracksIds;

	LMS_LOG(DBUPDATER, DEBUG) << "Extracting features...";
	for (auto track : tracks)
	{
		SOM::InputVector sample;

		if (!extractFeatures(track, trackFeatureTypes, sample))
			continue;

		samples.emplace_back(std::move(sample));
		tracksIds.emplace_back(track.id());
	}
	LMS_LOG(DBUPDATER, DEBUG) << "Extracting features DONE";

	transaction.commit();

	if (tracksIds.empty())
	{
		LMS_LOG(DBUPDATER, INFO) << "Nothing to classify!";
		std::atomic_store(&_finder, std::shared_ptr<SOMSearcher>());
		return;
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Normalizing data...";
	SOM::DataNormalizer normalizer(_featuresName.size());

	normalizer.computeNormalizationFactors(samples);
	for (auto& sample : samples)
		normalizer.normalizeData(sample);

	std::size_t size = std::sqrt(samples.size()/5);
	LMS_LOG(DBUPDATER, DEBUG) << "Found " << samples.size() << " tracks, Constructing a " << size << "*" << size << " network";
	SOM::Network network(size, size, _featuresName.size());

	LMS_LOG(DBUPDATER, DEBUG) << "Training network...";
	network.train(samples, 20);
	LMS_LOG(DBUPDATER, DEBUG) << "Training network DONE";

	// Now classify all the tracks
	LMS_LOG(DBUPDATER, DEBUG) << "Classifying tracks...";
	SOM::Matrix<std::vector<Database::IdType>> tracksMap(network.getWidth(), network.getHeight());
	std::map<Database::IdType, SOM::Coords> trackIdsCoords;

	for (std::size_t i = 0; i < samples.size(); ++i)
	{
		const auto& sample = samples[i];
		auto trackId = tracksIds[i];

		auto coords = network.classify(sample);
		tracksMap[coords].push_back(trackId);
		trackIdsCoords[trackId] = coords;
	}

	Similarity::SOMSearcher::ConstructionParams params{std::move(network), std::move(normalizer), std::move(tracksMap), std::move(trackIdsCoords)};

	auto finder = std::make_shared<Similarity::SOMSearcher>(std::move(params));

	std::atomic_store(&_finder, finder);

	LMS_LOG(DBUPDATER, DEBUG) << "Classifying tracks DONE";

	LMS_LOG(DBUPDATER, DEBUG) << "Dumping classifier:";

	std::ofstream ofs("/tmp/output");
	finder->dump(_db.getSession(), ofs);

	LMS_LOG(DBUPDATER, DEBUG) << "Dumping classifier DONE";

}

void
SOMScannerAddon::refreshSettings()
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	auto settings = Database::SimilaritySettings::get(_db.getSession());

	_settingsVersion = settings->getVersion();

	for (auto trackFeatureType : settings->getTrackFeatureTypes())
	{
		_featuresName.insert(trackFeatureType->getName());
	}
}

bool
SOMScannerAddon::fetchFeatures(Database::IdType trackId, const std::string& MBID)
{
	std::map<std::string, double> features;

	if (!AcousticBrainz::extractFeatures(MBID, _featuresName, features))
	{
		LMS_LOG(DBUPDATER, ERROR) << "Cannot extract features using AcousticBrainz!";
		return false;
	}

	Wt::Dbo::Transaction transaction(_db.getSession());

	Wt::Dbo::ptr<Database::Track> track = Database::Track::getById(_db.getSession(), trackId);
	if (!track)
		return false;

	LMS_LOG(DBUPDATER, DEBUG) << "Successfully extracted AcousticBrainz lowlevel features for track '" << track->getPath().string() << "'";

	for (const auto& feature : features)
	{
		auto featureType = Database::TrackFeatureType::getByName(_db.getSession(), feature.first);
		if (!featureType)
			return false;

		Database::TrackFeature::create(_db.getSession(), featureType, track, feature.second);
	}

	return true;
}

} // namespace Similarity

