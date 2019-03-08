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

struct FeatureInfo
{
	std::size_t	nbDimensions;
	double		weight;
};

using FeatureInfoMap = std::map<std::string, FeatureInfo>;

static
FeatureInfoMap
getFeatureInfoMap(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction {session};

	auto settings {Database::SimilaritySettings::get(session)};

	std::map<std::string, FeatureInfo> featuresInfo;
	for (auto feature : settings->getFeatures())
	{
		LMS_LOG(SIMILARITY, DEBUG) << "Feature '" << feature->getName() << "', nbDimns = " << feature->getNbDimensions() << ", weight = " << feature->getWeight() ;
		featuresInfo[feature->getName()] = { feature->getNbDimensions(), feature->getWeight() };
	}

	return featuresInfo;
}

static
std::size_t
getFeatureInfoMapNbDimensions(const FeatureInfoMap& featureInfoMap)
{
	return std::accumulate(featureInfoMap.begin(), featureInfoMap.end(), 0, [](std::size_t sum, auto it) { return sum + it.second.nbDimensions; });
}

FeaturesSearcher::FeaturesSearcher(Wt::Dbo::Session& session, bool& stopRequested)
{
	Wt::Dbo::Transaction transaction(session);

	FeatureInfoMap featuresInfo {getFeatureInfoMap(session)};
	std::size_t nbDimensions {getFeatureInfoMapNbDimensions(featuresInfo)};

	LMS_LOG(SIMILARITY, DEBUG) << "Features dimension = " << nbDimensions;

	LMS_LOG(SIMILARITY, DEBUG) << "Getting Tracks with features...";
	auto tracks {Database::Track::getAllWithFeatures(session)};
	LMS_LOG(SIMILARITY, DEBUG) << "Getting Tracks with features DONE";

	std::vector<SOM::InputVector> samples;
	std::vector<Database::IdType> tracksIds;

	LMS_LOG(SIMILARITY, DEBUG) << "Extracting features...";
	for (const Database::Track::pointer& track : tracks)
	{
		if (stopRequested)
			return;

		SOM::InputVector sample {nbDimensions};

		std::map<std::string, std::vector<double>> features;
		for (auto itFeatureInfo : featuresInfo)
			features[itFeatureInfo.first] = {};

		if (!track->getTrackFeatures()->getFeatures(features))
			continue;

		bool ok {true};
		std::size_t i {};
		for (const auto& feature : features)
		{
			// Check dimensions for each feature
			auto it {featuresInfo.find(feature.first)};
			if (it == featuresInfo.end() || it->second.nbDimensions != feature.second.size())
			{
				LMS_LOG(SIMILARITY, WARNING) << "Dimension mismatch for feature '" << feature.first << "'. Expected " << it->second.nbDimensions << ", got " << feature.second.size();
				ok = false;
				break;
			}

			for (double val : feature.second)
				sample[i++] = val;
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
	SOM::DataNormalizer dataNormalizer(nbDimensions);

	dataNormalizer.computeNormalizationFactors(samples);
	for (auto& sample : samples)
		dataNormalizer.normalizeData(sample);

	SOM::InputVector weights {nbDimensions};
	{
		std::size_t index {};
		for (const auto& featureInfo : featuresInfo)
		{
			for (std::size_t i {}; i < featureInfo.second.nbDimensions; ++i)
				weights[index++] = (1. / featureInfo.second.nbDimensions * featureInfo.second.weight);
		}
	}

	SOM::Coordinate size {static_cast<SOM::Coordinate>(std::sqrt(samples.size() / 4))};
	LMS_LOG(SIMILARITY, INFO) << "Found " << samples.size() << " tracks, constructing a " << size << "*" << size << " network";

	SOM::Network network {size, size, nbDimensions};
	std::cout << "Weights = '" << weights << "'";
	network.setDataWeights(weights);

	auto progressIndicator{[](const auto& iter)
		{
			LMS_LOG(SIMILARITY, DEBUG) << "Current pass = " << iter.idIteration << " / " << iter.iterationCount;
		}};

	auto stopper{[&]() { return stopRequested; }};

	LMS_LOG(SIMILARITY, DEBUG) << "Training network...";
	network.train(samples, 10, progressIndicator, stopper);
	LMS_LOG(SIMILARITY, DEBUG) << "Training network DONE";

	if (stopRequested)
		return;

	LMS_LOG(SIMILARITY, DEBUG) << "Classifying tracks...";
	std::map<Database::IdType, std::set<SOM::Position>> trackPositions;
	for (std::size_t i {}; i < samples.size(); ++i)
	{
		if (stopRequested)
			return;

		Wt::Dbo::Transaction transaction {session};

		const auto& sample = samples[i];
		auto trackId = tracksIds[i];
		auto position = network.getClosestRefVectorPosition(sample);

		trackPositions[trackId].insert(position);
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Classifying tracks DONE";

	init(session, std::move(network), std::move(trackPositions));
}

FeaturesSearcher::FeaturesSearcher(Wt::Dbo::Session& session, FeaturesCache cache)
{
	init(session, std::move(cache._network), std::move(cache._trackPositions));

	LMS_LOG(SIMILARITY, DEBUG) << "Init from cache DONE";
}

bool
FeaturesSearcher::isValid() const
{
	return _network.get() != nullptr;
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarTracks(const std::set<Database::IdType>& tracksIds, std::size_t maxCount) const
{
	return getSimilarObjects(tracksIds, _tracksMap, _trackPositions, maxCount);
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarReleases(Database::IdType releaseId, std::size_t maxCount) const
{
	return getSimilarObjects({releaseId}, _releasesMap, _releasePositions, maxCount);
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarArtists(Database::IdType artistId, std::size_t maxCount) const
{
	return getSimilarObjects({artistId}, _artistsMap, _artistPositions, maxCount);
}

void
FeaturesSearcher::dump(Wt::Dbo::Session& session, std::ostream& os) const
{
	if (!isValid())
	{
		os << "Invalid searcher" << std::endl;
		return;
	}

	os << "Number of tracks classified: " << _trackPositions.size() << std::endl;
	os << "Network size: " << _network->getWidth() << " * " << _network->getHeight() << std::endl;
	os << "Ref vectors median distance = " << _networkRefVectorsDistanceMedian << std::endl;

	Wt::Dbo::Transaction transaction(session);

	for (SOM::Coordinate y {}; y < _network->getHeight(); ++y)
	{
		for (SOM::Coordinate x {}; x < _network->getWidth(); ++x)
		{
			const auto& trackIds {_tracksMap[{x, y}]};

			os << "{" << x << ", " << y << "}";

			if (y > 0)
				os << " - {" << x << ", " << y - 1 << "}: " << _network->getRefVectorsDistance({x, y}, {x, y - 1});
			if (x > 0)
				os << " - {" << x - 1 << ", " << y << "}: " << _network->getRefVectorsDistance({x, y}, {x - 1, y});
			if (y !=  _network->getHeight() - 1)
				os << " - {" << x << ", " << y + 1 << "}: " << _network->getRefVectorsDistance({x, y}, {x, y + 1});
			if (x !=  _network->getWidth() - 1)
				os << " - {" << x + 1 << ", " << y << "}: " << _network->getRefVectorsDistance({x, y}, {x + 1, y});
			os << std::endl;

			for (Database::IdType trackId : trackIds)
			{
				auto track {Database::Track::getById(session, trackId)};
				if (!track)
					continue;

				os << "\t - " << track->getName() << " - ";
				if (track->getArtist())
					os << track->getArtist()->getName() << " - ";
				if (track->getRelease())
					os << track->getRelease()->getName();
				os << std::endl;
			}

		}
		os << std::endl;
	}
}

FeaturesCache
FeaturesSearcher::toCache() const
{
	return FeaturesCache{*_network, _trackPositions};
}

void
FeaturesSearcher::init(Wt::Dbo::Session& session,
			SOM::Network network,
			std::map<Database::IdType, std::set<SOM::Position>> tracksPosition)
{
	_network = std::make_unique<SOM::Network>(std::move(network));
	_networkRefVectorsDistanceMedian = _network->computeRefVectorsDistanceMedian();
	LMS_LOG(SIMILARITY, DEBUG) << "Median distance betweend ref vectors = " << _networkRefVectorsDistanceMedian;

	SOM::Coordinate width {_network->getWidth()};
	SOM::Coordinate height {_network->getHeight()};

	_artistsMap = SOM::Matrix<std::set<Database::IdType>>(width, height);
	_releasesMap = SOM::Matrix<std::set<Database::IdType>>(width, height);
	_tracksMap = SOM::Matrix<std::set<Database::IdType>>(width, height);

	Wt::Dbo::Transaction transaction {session};

	for (auto itTrackCoord : tracksPosition)
	{
		Database::IdType trackId {itTrackCoord.first};
		const std::set<SOM::Position>& positionSet {itTrackCoord.second};

		auto track {Database::Track::getById(session, trackId)};
		if (!track)
			continue;

		for (const SOM::Position& position : positionSet)
		{
			_tracksMap[position].insert(trackId);
			_trackPositions[trackId].insert(position);

			if (track->getRelease())
			{
				_releasePositions[track->getRelease().id()].insert(position);
				_releasesMap[position].insert(track->getRelease().id());
			}
			if (track->getArtist())
			{
				_artistPositions[track->getArtist().id()].insert(position);
				_artistsMap[position].insert(track->getArtist().id());
			}
		}
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Classifying tracks DONE";

}

static
std::set<SOM::Position>
getMatchingRefVectorsPosition(const std::set<Database::IdType>& ids, const std::map<Database::IdType, std::set<SOM::Position>>& objectPosition)
{
	std::set<SOM::Position> res;

	if (ids.empty())
		return res;

	for (auto id : ids)
	{
		auto it = objectPosition.find(id);
		if (it == objectPosition.end())
			continue;

		for (const auto& position : it->second)
			res.insert(position);
	}

	return res;
}

static
std::set<Database::IdType>
getObjectsIds(const std::set<SOM::Position>& positionSet, const SOM::Matrix<std::set<Database::IdType>>& objectsMap )
{
	std::set<Database::IdType> res;

	for (const auto& position : positionSet)
	{
		for (auto id : objectsMap.get(position))
			res.insert(id);
	}

	return res;
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarObjects(const std::set<Database::IdType>& ids,
		const SOM::Matrix<std::set<Database::IdType>>& objectsMap,
		const std::map<Database::IdType, std::set<SOM::Position>>& objectPosition,
		std::size_t maxCount) const
{
	std::vector<Database::IdType> res;

	if (!isValid())
		return res;

	auto now {std::chrono::system_clock::now()};
	std::mt19937 randGenerator{static_cast<std::mt19937::result_type>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count())};

	std::set<SOM::Position> searchedRefVectorsPosition {getMatchingRefVectorsPosition(ids, objectPosition)};
	if (searchedRefVectorsPosition.empty())
		return res;

	while (1)
	{
		std::set<Database::IdType> closestObjectIds {getObjectsIds(searchedRefVectorsPosition, objectsMap)};

		// Remove objects that are already in input or already reported
		for (auto id : ids)
			closestObjectIds.erase(id);

		for (auto id : res)
			closestObjectIds.erase(id);

		{
			std::vector<Database::IdType> objectIdsToAdd {closestObjectIds.begin(), closestObjectIds.end()};
			std::shuffle(objectIdsToAdd.begin(), objectIdsToAdd.end(), randGenerator);
			std::copy(objectIdsToAdd.begin(), objectIdsToAdd.end(), std::back_inserter(res));
		}

		if (res.size() > maxCount)
			res.resize(maxCount);

		if (res.size() == maxCount)
			break;

		// If there is not enough objects, try again with closest neighbour until there is too much distance
		boost::optional<SOM::Position> closestRefVectorPosition {_network->getClosestRefVectorPosition(searchedRefVectorsPosition, _networkRefVectorsDistanceMedian * 0.75)};
		if (!closestRefVectorPosition)
			break;

		searchedRefVectorsPosition.insert(*closestRefVectorPosition);
	}

	return res;
}



} // ns Similarity
