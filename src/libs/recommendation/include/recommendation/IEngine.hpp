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

#include <functional>
#include <unordered_set>

#include "database/Types.hpp"

namespace Database
{
	class Db;
	class Session;
}

namespace Recommendation
{
	class IEngine
	{
		public:
			virtual ~IEngine() = default;

			struct Progress
			{
				std::size_t totalElems {};
				std::size_t	processedElems {};
			};
			using ProgressCallback = std::function<void(const Progress&)>;
			virtual void load(bool forceReload, const ProgressCallback& progressCallback = {}) = 0;
			virtual void cancelLoad() = 0;

			virtual std::unordered_set<Database::IdType> getSimilarTracksFromTrackList(Database::Session& session, Database::IdType tracklistId, std::size_t maxCount) = 0;
			virtual std::unordered_set<Database::IdType> getSimilarTracks(Database::Session& session, const std::unordered_set<Database::IdType>& tracksId, std::size_t maxCount) = 0;
			virtual std::unordered_set<Database::IdType> getSimilarReleases(Database::Session& session, Database::IdType releaseId, std::size_t maxCount) = 0;
			virtual std::unordered_set<Database::IdType> getSimilarArtists(Database::Session& session, Database::IdType artistId, std::size_t maxCount) = 0;
	};

	std::unique_ptr<IEngine> createEngine(Database::Db& db);

} // ns Recommendation

