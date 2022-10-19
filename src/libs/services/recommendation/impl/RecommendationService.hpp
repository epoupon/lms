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

#pragma once

#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "services/recommendation/IRecommendationService.hpp"
#include "IEngine.hpp"

namespace Database
{
	class Db;
}

namespace Recommendation
{
	enum class EngineType
	{
		Clusters,
		Features,
	};

	class RecommendationService : public IRecommendationService
	{
		public:
			RecommendationService(Database::Db& db);
			~RecommendationService() = default;

			RecommendationService(const RecommendationService&) = delete;
			RecommendationService(RecommendationService&&) = delete;
			RecommendationService& operator=(const RecommendationService&) = delete;
			RecommendationService& operator=(RecommendationService&&) = delete;

		private:
			void	load(bool forceReload, const ProgressCallback& progressCallback) override;
			void	cancelLoad() override;

			TrackContainer findSimilarTracks(Database::TrackListId tracklistId, std::size_t maxCount) const override;
			TrackContainer findSimilarTracks(const std::vector<Database::TrackId>& tracksId, std::size_t maxCount) const override;
			ReleaseContainer getSimilarReleases(Database::ReleaseId releaseId, std::size_t maxCount) const override;
			ArtistContainer getSimilarArtists(Database::ArtistId artistId, EnumSet<Database::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

			void setEnginePriorities(const std::vector<EngineType>& engineTypes);
			void clearEngines();
			void loadPendingEngine(EngineType engineType, std::unique_ptr<IEngine> engine, bool forceReload, const ProgressCallback& progressCallback);

			Database::Db&				_db;

			std::mutex					_controlMutex;
			bool						_loadCancelled {};

			using EngineContainer = std::unordered_map<EngineType, std::unique_ptr<IEngine>>;
			EngineContainer				_engines;
			mutable std::shared_mutex	_enginesMutex;

			std::vector<IEngine*>		_pendingEngines;
			std::shared_mutex			_pendingEnginesMutex;
			std::condition_variable 	_pendingEnginesCondvar;

			std::vector<EngineType>		_enginePriorities; // ordered by priority
	};

} // ns Recommendation

