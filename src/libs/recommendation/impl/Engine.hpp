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
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "recommendation/IEngine.hpp"
#include "IClassifier.hpp"

namespace Database
{
	class Db;
}

namespace Recommendation
{
	enum class ClassifierType
	{
		Clusters,
		Features,
	};

	class Engine : public IEngine
	{
		public:
			Engine(Database::Db& db);
			~Engine() = default;

			Engine(const Engine&) = delete;
			Engine(Engine&&) = delete;
			Engine& operator=(const Engine&) = delete;
			Engine& operator=(Engine&&) = delete;

		private:
			void	load(bool forceReload, const ProgressCallback& progressCallback) override;
			void	cancelLoad() override;

			ResultContainer getSimilarTracksFromTrackList(Database::Session& session, Database::IdType tracklistId, std::size_t maxCount) override;
			ResultContainer getSimilarTracks(Database::Session& session, const std::unordered_set<Database::IdType>& tracksId, std::size_t maxCount) override;
			ResultContainer getSimilarReleases(Database::Session& session, Database::IdType releaseId, std::size_t maxCount) override;
			ResultContainer getSimilarArtists(Database::Session& session,
					Database::IdType artistId,
					EnumSet<Database::TrackArtistLinkType> linkTypes,
					std::size_t maxCount) override;

			void setClassifierPriorities(const std::vector<ClassifierType>& classifierTypes);
			void clearClassifiers();
			void loadClassifier(std::unique_ptr<IClassifier> classifier, ClassifierType classifierType, bool forceReload, const ProgressCallback& progressCallback);

			Database::Db&				_db;

			std::mutex							_controlMutex;
			bool								_loadCancelled {};
			std::condition_variable 			_pendingClassifiersCondvar;
			std::unordered_set<IClassifier*>	_pendingClassifiers;

			std::shared_mutex			_classifiersMutex;
			using ClassifierContainer = std::unordered_map<ClassifierType, std::unique_ptr<IClassifier>>;
			ClassifierContainer			_classifiers;
			std::vector<ClassifierType>	_classifierPriorities; // ordered by priority

	};

} // ns Recommendation

