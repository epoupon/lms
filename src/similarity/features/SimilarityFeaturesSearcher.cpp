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

#include "SimilarityFeaturesSearcher.hpp"

#include <random>

#include "database/Artist.hpp"
#include "database/SimilaritySettings.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "database/TrackFeatures.hpp"
#include "som/DataNormalizer.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"


namespace Similarity {


FeaturesSearcher::FeaturesSearcher(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction(session);

	auto settings = Database::SimilaritySettings::get(session);

	struct FeatureInfo
	{
		std::size_t	nbDimensions;
		double		weight;
	};

	std::map<std::string, FeatureInfo> featuresInfo;
	std::size_t nbDimensions = 0;
	for (auto feature : settings->getFeatures())
	{
		featuresInfo[feature->getName()] = { feature->getNbDimensions(), feature->getWeight() };
		nbDimensions += feature->getNbDimensions();
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Getting Tracks with features...";
	auto tracks = Database::Track::getAllWithFeatures(session);
	LMS_LOG(SIMILARITY, DEBUG) << "Getting Tracks with features DONE";

	std::vector<SOM::InputVector> samples;
	std::vector<Database::IdType> tracksIds;

	LMS_LOG(SIMILARITY, DEBUG) << "Extracting features...";
	for (auto track : tracks)
	{
		SOM::InputVector sample;

		std::map<std::string, std::vector<double>> features;
		for (const auto& featureInfo : featuresInfo)
			features[featureInfo.first] = {};

		if (!track->getTrackFeatures()->getFeatures(features))
			continue;

		// Check dimensions for each feature
		bool ok = true;
		for (const auto& feature : features)
		{
			auto it = featuresInfo.find(feature.first);
			if (it == featuresInfo.end() || it->second.nbDimensions != feature.second.size())
			{
				LMS_LOG(SIMILARITY, WARNING) << "Dimension mismatch for feature '" << feature.first << "'. Expected " << it->second.nbDimensions << ", got " << feature.second.size();
				ok = false;
				break;
			}

			sample.insert( sample.end(), feature.second.begin(), feature.second.end() );
		}

		if (!ok)
			continue;

		samples.emplace_back(std::move(sample));
		tracksIds.emplace_back(track.id());
	}
	LMS_LOG(SIMILARITY, DEBUG) << "Extracting features DONE";

	transaction.commit();

	if (tracksIds.empty())
	{
		LMS_LOG(SIMILARITY, INFO) << "Nothing to classify!";
		return;
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Normalizing data...";
	SOM::DataNormalizer normalizer(nbDimensions);

	normalizer.computeNormalizationFactors(samples);
	for (auto& sample : samples)
		normalizer.normalizeData(sample);

	std::size_t size = std::sqrt(samples.size()/2);
	LMS_LOG(SIMILARITY, INFO) << "Found " << samples.size() << " tracks, constructing a " << size << "*" << size << " network";

	_network = std::make_unique<SOM::Network>(size, size, nbDimensions);
	_artistsMap = SOM::Matrix<std::set<Database::IdType>>(size, size);
	_releasesMap = SOM::Matrix<std::set<Database::IdType>>(size, size);
	_tracksMap = SOM::Matrix<std::set<Database::IdType>>(size, size);

	std::vector<double> weights;
	for (const auto& featureInfo : featuresInfo)
	{
		for (std::size_t i = 0; i < featureInfo.second.nbDimensions; ++i)
			weights.push_back(1. / featureInfo.second.nbDimensions * featureInfo.second.weight);
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Training network...";
	_network->train(samples, 20);
	LMS_LOG(SIMILARITY, DEBUG) << "Training network DONE";

	LMS_LOG(SIMILARITY, DEBUG) << "Classifying tracks...";

	for (std::size_t i = 0; i < samples.size(); ++i)
	{
		Wt::Dbo::Transaction transaction(session);

		const auto& sample = samples[i];
		auto trackId = tracksIds[i];

		auto coords = _network->getClosestRefVectorCoords(sample);

		_trackCoords[trackId].insert(coords);
		_tracksMap[coords].insert(trackId);

		auto track = Database::Track::getById(session, trackId);
		if (track->getRelease())
		{
			_releaseCoords[track->getRelease().id()].insert(coords);
			_releasesMap[coords].insert(track->getRelease().id());
		}
		if (track->getArtist())
		{
			_artistCoords[track->getArtist().id()].insert(coords);
			_artistsMap[coords].insert(track->getArtist().id());
		}
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Classifying tracks DONE";

}


std::vector<Database::IdType>
FeaturesSearcher::getSimilarTracks(const std::set<Database::IdType>& tracksIds, std::size_t maxCount) const
{
	return getSimilarObjects(tracksIds, _tracksMap, _trackCoords, maxCount);
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarReleases(Database::IdType releaseId, std::size_t maxCount) const
{
	return getSimilarObjects({releaseId}, _releasesMap, _releaseCoords, maxCount);
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarArtists(Database::IdType artistId, std::size_t maxCount) const
{
	return getSimilarObjects({artistId}, _artistsMap, _artistCoords, maxCount);
}
#if 0
void
FeaturesSearcher::dump(Wt::Dbo::Session& session, std::ostream& os) const
{
	os << "Number of tracks classified: " << _trackIdsCoords.size() << std::endl;
	os << "Network size: " << _network.getWidth() << " * " << _network.getHeight() << std::endl;

	Wt::Dbo::Transaction transaction(session);

	for (std::size_t y = 0; y < _network.getHeight(); ++y)
	{
		for (std::size_t x = 0; x < _network.getWidth(); ++x)
		{
			const auto& trackIds = _tracksMap[{x, y}];

			for (auto trackId : trackIds)
			{
				auto track = Database::Track::getById(session, trackId);
				if (!track)
					continue;

				os << "{";
				if (track->getArtist())
					os << track->getArtist()->getName() << " ";
				if (track->getRelease())
					os << track->getRelease()->getName();
				os << "} ";
			}

			os << "; ";
		}
		os << std::endl;
	}
}
#endif


static
std::set<SOM::Coords>
getMatchingRefVectorsCoords(const std::set<Database::IdType>& ids, const std::map<Database::IdType, std::set<SOM::Coords>>& objectCoords)
{
	std::set<SOM::Coords> res;

	if (ids.empty())
		return res;

	for (auto id : ids)
	{
		auto it = objectCoords.find(id);
		if (it == objectCoords.end())
			continue;

		for (const auto& coords : it->second)
			res.insert(coords);
	}

	return res;
}

static
std::set<Database::IdType>
getObjectsIds(const std::set<SOM::Coords>& coordsSet, const SOM::Matrix<std::set<Database::IdType>>& objectsMap )
{
	std::set<Database::IdType> res;

	for (const auto& coords : coordsSet)
	{
		for (auto id : objectsMap.get(coords))
			res.insert(id);
	}

	return res;
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarObjects(const std::set<Database::IdType>& ids,
		const SOM::Matrix<std::set<Database::IdType>>& objectsMap,
		const std::map<Database::IdType, std::set<SOM::Coords>>& objectCoords,
		std::size_t maxCount) const
{
	std::vector<Database::IdType> res;

	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	std::set<SOM::Coords> searchedRefVectorsCoords = getMatchingRefVectorsCoords(ids, objectCoords);
	if (searchedRefVectorsCoords.empty())
		return res;

	while (1)
	{
		std::set<Database::IdType> closestObjectIds = getObjectsIds(searchedRefVectorsCoords, objectsMap);

		// Remove objects that are already in input
		for (auto id : ids)
			closestObjectIds.erase(id);

		{
			std::vector<Database::IdType> objectIdsToAdd(closestObjectIds.begin(), closestObjectIds.end());

			std::shuffle(objectIdsToAdd.begin(), objectIdsToAdd.end(), randGenerator);
			std::copy(objectIdsToAdd.begin(), objectIdsToAdd.end(), std::back_inserter(res));
		}

		if (res.size() > maxCount)
			res.resize(maxCount);

		if (res.size() == maxCount)
			break;

		// If there is not enough objects, try again with closest neighbour until there is too much distance
		auto closestRefVectorCoords = _network->getClosestRefVectorCoords(searchedRefVectorsCoords, _networkRefVectorsDistanceMedian * 0.75);
		if (!closestRefVectorCoords)
			break;

		searchedRefVectorsCoords.insert(*closestRefVectorCoords);
	}

	return res;
}



} // ns Similarity
