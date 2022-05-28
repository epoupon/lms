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

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

#include "som/DataNormalizer.hpp"
#include "som/Network.hpp"
#include "utils/Utils.hpp"
#include "IEngine.hpp"
#include "FeaturesEngineCache.hpp"
#include "FeaturesDefs.hpp"

namespace Database
{
	class Session;
}

namespace Recommendation {

using FeatureWeight = double;

class FeaturesEngine : public IEngine
{
	public:
		FeaturesEngine(Database::Db& db) : _db {db} {}

		FeaturesEngine(const FeaturesEngine&) = delete;
		FeaturesEngine(FeaturesEngine&&) = delete;
		FeaturesEngine& operator=(const FeaturesEngine&) = delete;
		FeaturesEngine& operator=(FeaturesEngine&&) = delete;

		static const FeatureSettingsMap& getDefaultTrainFeatureSettings();

	private:
		void load(bool forceReload, const ProgressCallback& progressCallback) override;
		void requestCancelLoad() override;

		TrackContainer findSimilarTracksFromTrackList(Database::TrackListId tracklistId, std::size_t maxCount) const override;
		TrackContainer findSimilarTracks(const std::vector<Database::TrackId>& tracksId, std::size_t maxCount) const override;
		ReleaseContainer getSimilarReleases(Database::ReleaseId releaseId, std::size_t maxCount) const override;
		ArtistContainer getSimilarArtists(Database::ArtistId artistId, EnumSet<Database::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

		void loadFromCache(FeaturesEngineCache&& cache);

		// Use training (may be very slow)
		struct TrainSettings
		{
			std::size_t iterationCount {10};
			float sampleCountPerNeuron {4};
			FeatureSettingsMap featureSettingsMap;
		};
		void loadFromTraining(const TrainSettings& trainSettings, const ProgressCallback& progressCallback);

		template <typename IdType>
		using ObjectPositions = std::unordered_map<IdType, std::vector<SOM::Position>>;

		using ArtistPositions = ObjectPositions<Database::ArtistId>;
		using ReleasePositions = ObjectPositions<Database::ReleaseId>;
		using TrackPositions = ObjectPositions<Database::TrackId>;

		template <typename IdType>
		using ObjectMatrix = SOM::Matrix<std::vector<IdType>>;
		using ArtistMatrix = ObjectMatrix<Database::ArtistId>;
		using ReleaseMatrix = ObjectMatrix<Database::ReleaseId>;
		using TrackMatrix = ObjectMatrix<Database::TrackId>;

		void load(const SOM::Network& network, const TrackPositions& tracksPosition);

		FeaturesEngineCache toCache() const;

		template <typename IdType>
		static std::vector<SOM::Position> getMatchingRefVectorsPosition(const std::vector<IdType>& ids, const ObjectPositions<IdType>& objectPositions);

		template <typename IdType>
		static std::vector<IdType> getObjectsIds(const std::vector<SOM::Position>& positions, const ObjectMatrix<IdType>& objectsMatrix);

		template <typename IdType>
		std::vector<IdType> getSimilarObjects(const std::vector<IdType>& ids,
				const ObjectMatrix<IdType>& objectMatrix,
				const ObjectPositions<IdType>& objectPositions,
				std::size_t maxCount) const;

		Database::Db&		_db;
		bool				_loadCancelled {};
		std::unique_ptr<SOM::Network>	_network;
		double				_networkRefVectorsDistanceMedian {};

		ArtistPositions     _artistPositions;
		std::unordered_map<Database::TrackArtistLinkType, ArtistMatrix> _artistMatrix;

		ReleasePositions	_releasePositions;
		ReleaseMatrix		_releaseMatrix;

		TrackPositions		_trackPositions;
		TrackMatrix			_trackMatrix;
};

template <typename IdType>
std::vector<SOM::Position>
FeaturesEngine::getMatchingRefVectorsPosition(const std::vector<IdType>& ids, const ObjectPositions<IdType>& objectPositions)
{
	std::vector<SOM::Position> res;

	if (ids.empty())
		return res;

	for (const IdType id : ids)
	{
		auto it = objectPositions.find(id);
		if (it == objectPositions.end())
			continue;

		for (const SOM::Position& position : it->second)
			Utils::push_back_if_not_present(res, position);
	}

	return res;
}

template <typename IdType>
std::vector<IdType>
FeaturesEngine::getObjectsIds(const std::vector<SOM::Position>& positions, const ObjectMatrix<IdType>& objectMatrix)
{
	std::vector<IdType> res;

	for (const SOM::Position& position : positions)
	{
		for (const IdType id : objectMatrix.get(position))
			Utils::push_back_if_not_present(res, id);
	}

	return res;
}

template <typename IdType>
std::vector<IdType>
FeaturesEngine::getSimilarObjects(const std::vector<IdType>& ids,
		const ObjectMatrix<IdType>& objectMatrix,
		const ObjectPositions<IdType>& objectPositions,
		std::size_t maxCount) const
{
	std::vector<IdType> res;

	std::vector<SOM::Position> searchedRefVectorsPosition {getMatchingRefVectorsPosition(ids, objectPositions)};
	if (searchedRefVectorsPosition.empty())
		return res;

	while (1)
	{
		std::vector<IdType> closestObjectIds {getObjectsIds(searchedRefVectorsPosition, objectMatrix)};

		// Remove objects that are already in input or already reported
		closestObjectIds.erase(std::remove_if(std::begin(closestObjectIds), std::end(closestObjectIds),
					[&](IdType id)
					{
						return std::find(std::cbegin(ids), std::cend(ids), id) != std::cend(ids);
					})
				, std::end(closestObjectIds));

		for (IdType id : closestObjectIds)
		{
			if (res.size() == maxCount)
				break;

			Utils::push_back_if_not_present(res, id);
		}

		if (res.size() == maxCount)
			break;

		// If there is not enough objects, try again with closest neighbour until there is too much distance
		const std::optional<SOM::Position> closestRefVectorPosition {_network->getClosestRefVectorPosition(searchedRefVectorsPosition, _networkRefVectorsDistanceMedian * 0.75)};
		if (!closestRefVectorPosition)
			break;

		Utils::push_back_if_not_present(searchedRefVectorsPosition, closestRefVectorPosition.value());
	}

	return res;
}

} // ns Recommendation
