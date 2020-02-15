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

#include <vector>
#include <unordered_set>

#include "database/Types.hpp"
#include "Classifier.hpp"

namespace Database
{
	class Session;
}

namespace Recommendation
{
	class Classifier;
	class IEngine
	{
		public:
			virtual ~IEngine() = default;

			// Closest results first
			virtual std::vector<Database::IdType> getSimilarTracksFromTrackList(Database::Session& session, Database::IdType tracklistId, std::size_t maxCount) = 0;
			virtual std::vector<Database::IdType> getSimilarTracks(Database::Session& session, const std::unordered_set<Database::IdType>& tracksId, std::size_t maxCount) = 0;
			virtual std::vector<Database::IdType> getSimilarReleases(Database::Session& session, Database::IdType releaseId, std::size_t maxCount) = 0;
			virtual std::vector<Database::IdType> getSimilarArtists(Database::Session& session, Database::IdType artistId, std::size_t maxCount) = 0;
	};

	std::unique_ptr<IEngine> createEngine();

} // ns Recommendation

