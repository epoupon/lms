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

#include "FeaturesClassifier.hpp"

#include <numeric>

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackFeatures.hpp"
#include "database/TrackList.hpp"
#include "som/DataNormalizer.hpp"
#include "utils/Logger.hpp"
#include "utils/Random.hpp"


namespace Recommendation {

std::unique_ptr<IClassifier> createFeaturesClassifier()
{
	return std::make_unique<FeaturesClassifier>();
}

const FeatureSettingsMap&
FeaturesClassifier::getDefaultTrainFeatureSettings()
{
	static const FeatureSettingsMap defaultTrainFeatureSettings
	{
		{ "lowlevel.spectral_energyband_high.mean",	{1}},
		{ "lowlevel.spectral_rolloff.median",		{1}},
		{ "lowlevel.spectral_contrast_valleys.var",	{1}},
		{ "lowlevel.erbbands.mean",			{1}},
		{ "lowlevel.gfcc.mean",				{1}},
	};

	return defaultTrainFeatureSettings;
}

static
std::optional<FeatureValuesMap>
getTrackFeatureValues(FeaturesClassifier::FeaturesFetchFunc func, Database::IdType trackId, const std::unordered_set<FeatureName>& featureNames)
{
	return func(trackId, featureNames);
}

static
std::optional<FeatureValuesMap>
getTrackFeatureValuesFromDb(Database::Session& session, Database::IdType trackId, const std::unordered_set<FeatureName>& featureNames)
{
	auto func = [&](Database::IdType trackId, const std::unordered_set<FeatureName>& featureNames)
	{
		std::optional<FeatureValuesMap> res;

		auto transaction {session.createSharedTransaction()};

		Database::Track::pointer track {Database::Track::getById(session, trackId)};
		if (!track)
			return res;

		res = track->getTrackFeatures()->getFeatureValuesMap(featureNames);
		if (res->empty())
			res.reset();

		return res;
	};

	return getTrackFeatureValues(func, trackId, featureNames);
}

static
std::optional<SOM::InputVector>
convertFeatureValuesMapToInputVector(const FeatureValuesMap& featureValuesMap, std::size_t nbDimensions)
{
	std::size_t i {};
	std::optional<SOM::InputVector> res {SOM::InputVector {nbDimensions}};
	for (const auto& [featureName, values] : featureValuesMap)
	{
		if (values.size() != getFeatureDef(featureName).nbDimensions)
		{
			LMS_LOG(RECOMMENDATION, WARNING) << "Dimension mismatch for feature '" << featureName << "'. Expected " << getFeatureDef(featureName).nbDimensions << ", got " << values.size();
			res.reset();
			break;
		}

		for (double val : values)
			(*res)[i++] = val;
	}

	return res;
}

static
SOM::InputVector
getInputVectorWeights(const FeatureSettingsMap& featureSettingsMap, std::size_t nbDimensions)
{
	SOM::InputVector weights {nbDimensions};
	std::size_t index {};
	for (const auto& [featureName, featureSettings] : featureSettingsMap)
	{
		const std::size_t featureNbDimensions {getFeatureDef(featureName).nbDimensions};

		for (std::size_t i {}; i < featureNbDimensions; ++i)
			weights[index++] = (1. / featureNbDimensions * featureSettings.weight);
	}

	assert(index == nbDimensions);

	return weights;
}

bool
FeaturesClassifier::initFromTraining(Database::Session& session, const TrainSettings& trainSettings)
{
	LMS_LOG(RECOMMENDATION, INFO) << "Constructing features classifier...";

	std::unordered_set<FeatureName> featureNames;
	std::transform(std::cbegin(trainSettings.featureSettingsMap), std::cend(trainSettings.featureSettingsMap), std::inserter(featureNames, std::begin(featureNames)),
		[](const auto& itFeatureSetting) { return itFeatureSetting.first; });

	const std::size_t nbDimensions {std::accumulate(std::cbegin(featureNames), std::cend(featureNames), std::size_t {0},
			[](std::size_t sum, const FeatureName& featureName) { return sum + getFeatureDef(featureName).nbDimensions; })};

	LMS_LOG(RECOMMENDATION, DEBUG) << "Features dimension = " << nbDimensions;

	std::vector<Database::IdType> trackIds;
	{
		auto transaction {session.createSharedTransaction()};

		LMS_LOG(RECOMMENDATION, DEBUG) << "Getting Tracks with features...";
		trackIds = Database::Track::getAllIdsWithFeatures(session);
		LMS_LOG(RECOMMENDATION, DEBUG) << "Getting Tracks with features DONE (found " << trackIds.size() << " tracks)";
	}

	std::vector<SOM::InputVector> samples;
	std::vector<Database::IdType> samplesTrackIds;

	samples.reserve(trackIds.size());
	samplesTrackIds.reserve(trackIds.size());

	LMS_LOG(RECOMMENDATION, DEBUG) << "Extracting features...";
	for (Database::IdType trackId : trackIds)
	{
		if (_initCancelled)
			return false;

		std::optional<FeatureValuesMap> featureValuesMap;

		if (_featuresFetchFunc)
			featureValuesMap = getTrackFeatureValues(_featuresFetchFunc, trackId, featureNames);
		else
			featureValuesMap = getTrackFeatureValuesFromDb(session, trackId, featureNames);

		if (!featureValuesMap)
			continue;

		std::optional<SOM::InputVector> inputVector {convertFeatureValuesMapToInputVector(*featureValuesMap, nbDimensions)};
		if (!inputVector)
			continue;

		samples.emplace_back(std::move(*inputVector));
		samplesTrackIds.emplace_back(trackId);
	}
	LMS_LOG(RECOMMENDATION, DEBUG) << "Extracting features DONE";

	if (samples.empty())
	{
		LMS_LOG(RECOMMENDATION, INFO) << "Nothing to classify!";
		return false;
	}

	LMS_LOG(RECOMMENDATION, DEBUG) << "Normalizing data...";
	SOM::DataNormalizer dataNormalizer {nbDimensions};

	dataNormalizer.computeNormalizationFactors(samples);
	for (auto& sample : samples)
		dataNormalizer.normalizeData(sample);

	const SOM::Coordinate size {static_cast<SOM::Coordinate>(std::sqrt(samples.size() / trainSettings.sampleCountPerNeuron))};
	LMS_LOG(RECOMMENDATION, INFO) << "Found " << samples.size() << " tracks, constructing a " << size << "*" << size << " network";

	SOM::Network network {size, size, nbDimensions};

	SOM::InputVector weights {getInputVectorWeights(trainSettings.featureSettingsMap, nbDimensions)};
	network.setDataWeights(weights);

	auto progressIndicator{[](const auto& iter)
		{
			LMS_LOG(RECOMMENDATION, DEBUG) << "Current pass = " << iter.idIteration << " / " << iter.iterationCount;
		}};

	LMS_LOG(RECOMMENDATION, DEBUG) << "Training network...";
	network.train(samples, trainSettings.iterationCount, progressIndicator);
	LMS_LOG(RECOMMENDATION, DEBUG) << "Training network DONE";

	if (_initCancelled)
		return false;

	LMS_LOG(RECOMMENDATION, DEBUG) << "Classifying tracks...";
	ObjectPositions trackPositions;
	for (std::size_t i {}; i < samples.size(); ++i)
	{
		if (_initCancelled)
			return false;

		const SOM::Position position {network.getClosestRefVectorPosition(samples[i])};

		trackPositions[samplesTrackIds[i]].insert(position);
	}

	LMS_LOG(RECOMMENDATION, DEBUG) << "Classifying tracks DONE";

	return init(session, std::move(network), std::move(trackPositions));
}

bool
FeaturesClassifier::initFromCache(Database::Session& session, const FeaturesClassifierCache& cache)
{
	LMS_LOG(RECOMMENDATION, INFO) << "Constructing features classifier from cache...";

	return init(session, std::move(cache._network), cache._trackPositions);
}

std::vector<Database::IdType>
FeaturesClassifier::getSimilarTracksFromTrackList(Database::Session& session, Database::IdType trackListId, std::size_t maxCount) const
{
	const std::unordered_set<Database::IdType> trackIds {[&]() -> std::unordered_set<Database::IdType>
	{
		auto transaction {session.createSharedTransaction()};

		const Database::TrackList::pointer trackList {Database::TrackList::getById(session, trackListId)};
		if (trackList)
		{
			const std::vector<Database::IdType> orderedTrackIds {trackList->getTrackIds()};
			return std::unordered_set<Database::IdType> {std::cbegin(orderedTrackIds), std::cend(orderedTrackIds)};
		}

		return {};
	}()};

	return getSimilarTracks(session, trackIds, maxCount);
}

std::vector<Database::IdType>
FeaturesClassifier::getSimilarTracks(Database::Session& session, const std::unordered_set<Database::IdType>& tracksIds, std::size_t maxCount) const
{
	std::vector<Database::IdType> similarTrackIds {getSimilarObjects(tracksIds, _tracksMap, _trackPositions, maxCount)};

	if (!similarTrackIds.empty())
	{
		// Report only existing ids
		auto transaction {session.createSharedTransaction()};

		similarTrackIds.erase(std::remove_if(std::begin(similarTrackIds), std::end(similarTrackIds),
					[&](Database::IdType trackId) { return Database::Track::getById(session, trackId) == Database::Track::pointer {}; }),
				std::cend(similarTrackIds));
	}

	return similarTrackIds;
}

std::vector<Database::IdType>
FeaturesClassifier::getSimilarReleases(Database::Session& session, Database::IdType releaseId, std::size_t maxCount) const
{
	std::vector<Database::IdType> similarReleaseIds {getSimilarObjects({releaseId}, _releasesMap, _releasePositions, maxCount)};

	if (!similarReleaseIds.empty())
	{
		// Report only existing ids
		auto transaction {session.createSharedTransaction()};

		similarReleaseIds.erase(std::remove_if(std::begin(similarReleaseIds), std::end(similarReleaseIds),
					[&](Database::IdType releaseId) { return Database::Release::getById(session, releaseId) == Database::Release::pointer {}; }),
				std::cend(similarReleaseIds));
	}

	return similarReleaseIds;
}

std::vector<Database::IdType>
FeaturesClassifier::getSimilarArtists(Database::Session& session, Database::IdType artistId, std::size_t maxCount) const
{
	std::vector<Database::IdType> similarArtistIds {getSimilarObjects({artistId}, _artistsMap, _artistPositions, maxCount)};

	if (!similarArtistIds.empty())
	{
		// Report only existing ids
		auto transaction {session.createSharedTransaction()};

		similarArtistIds.erase(std::remove_if(std::begin(similarArtistIds), std::end(similarArtistIds),
					[&](Database::IdType artistId) { return Database::Artist::getById(session, artistId) == Database::Artist::pointer {}; }),
				std::cend(similarArtistIds));
	}

	return similarArtistIds;
}

FeaturesClassifierCache
FeaturesClassifier::toCache() const
{
	return FeaturesClassifierCache {*_network, _trackPositions};
}

bool
FeaturesClassifier::init(Database::Session& session, bool databaseChanged)
{
	if (databaseChanged)
	{
		LMS_LOG(RECOMMENDATION, DEBUG) << "Database changed: invidating cache";
		FeaturesClassifierCache::invalidate();
	}

	std::optional<FeaturesClassifierCache> cache {FeaturesClassifierCache::read()};
	if (cache)
		return initFromCache(session, *cache);

	TrainSettings trainSettings;
	trainSettings.featureSettingsMap = getDefaultTrainFeatureSettings();

	bool res {initFromTraining(session, trainSettings)};
	if (res)
		toCache().write();

	return res;
}

void
FeaturesClassifier::requestCancelInit()
{
	LMS_LOG(RECOMMENDATION, DEBUG) << "Requesting init cancellation";
	_initCancelled = true;
}

bool
FeaturesClassifier::init(Database::Session& session,
			SOM::Network network,
			const ObjectPositions& tracksPosition)
{
	_networkRefVectorsDistanceMedian = network.computeRefVectorsDistanceMedian();
	LMS_LOG(RECOMMENDATION, DEBUG) << "Median distance betweend ref vectors = " << _networkRefVectorsDistanceMedian;

	const SOM::Coordinate width {network.getWidth()};
	const SOM::Coordinate height {network.getHeight()};

	_artistsMap = MatrixOfObjects {width, height};
	_releasesMap = MatrixOfObjects {width, height};
	_tracksMap = MatrixOfObjects {width, height};

	LMS_LOG(RECOMMENDATION, DEBUG) << "Constructing maps...";

	for (auto itTrackCoord : tracksPosition)
	{
		if (_initCancelled)
			return false;

		auto transaction {session.createSharedTransaction()};

		Database::IdType trackId {itTrackCoord.first};
		const std::unordered_set<SOM::Position>& positionSet {itTrackCoord.second};

		const Database::Track::pointer track {Database::Track::getById(session, trackId)};
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
			for (const auto& artist : track->getArtists())
			{
				_artistPositions[artist.id()].insert(position);
				_artistsMap[position].insert(artist.id());
			}
		}
	}

	_network = std::make_unique<SOM::Network>(std::move(network));

	LMS_LOG(RECOMMENDATION, INFO) << "Classifier successfully initialized!";

	return true;
}

std::unordered_set<SOM::Position>
FeaturesClassifier::getMatchingRefVectorsPosition(const std::unordered_set<Database::IdType>& ids, const ObjectPositions& objectPositions)
{
	std::unordered_set<SOM::Position> res;

	if (ids.empty())
		return res;

	for (auto id : ids)
	{
		auto it = objectPositions.find(id);
		if (it == objectPositions.end())
			continue;

		for (const auto& position : it->second)
			res.insert(position);
	}

	return res;
}

std::unordered_set<Database::IdType>
FeaturesClassifier::getObjectsIds(const std::unordered_set<SOM::Position>& positionSet, const MatrixOfObjects& objectsMap)
{
	std::unordered_set<Database::IdType> res;

	for (const auto& position : positionSet)
	{
		for (auto id : objectsMap.get(position))
			res.insert(id);
	}

	return res;
}

std::vector<Database::IdType>
FeaturesClassifier::getSimilarObjects(const std::unordered_set<Database::IdType>& ids,
		const MatrixOfObjects& objectsMap,
		const ObjectPositions& objectPosition,
		std::size_t maxCount) const
{
	std::vector<Database::IdType> res;

	std::unordered_set<SOM::Position> searchedRefVectorsPosition {getMatchingRefVectorsPosition(ids, objectPosition)};
	if (searchedRefVectorsPosition.empty())
		return res;

	while (1)
	{
		std::unordered_set<Database::IdType> closestObjectIds {getObjectsIds(searchedRefVectorsPosition, objectsMap)};

		// Remove objects that are already in input or already reported
		for (auto id : ids)
			closestObjectIds.erase(id);

		{
			std::vector<Database::IdType> objectIdsToAdd {std::cbegin(closestObjectIds), std::cend(closestObjectIds)};
			Random::shuffleContainer(objectIdsToAdd );
			std::copy(std::cbegin(objectIdsToAdd), std::cend(objectIdsToAdd), std::back_inserter(res));
		}

		if (res.size() > maxCount)
			res.resize(maxCount);

		if (res.size() == maxCount)
			break;

		// If there is not enough objects, try again with closest neighbour until there is too much distance
		const std::optional<SOM::Position> closestRefVectorPosition {_network->getClosestRefVectorPosition(searchedRefVectorsPosition, _networkRefVectorsDistanceMedian * 0.75)};
		if (!closestRefVectorPosition)
			break;

		searchedRefVectorsPosition.insert(closestRefVectorPosition.value());
	}

	return res;
}



} // ns Recommendation
