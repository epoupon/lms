/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <unordered_set>
#include <vector>

#include "database/Types.hpp"

namespace Database
{
	class Session;
}

namespace Recommendation
{

	class Classifier
	{
		public:
			virtual ~Classifier() = default;

			virtual void classify() = 0;

			virtual bool isTrackClassified(Database::IdType trackId) const = 0;
			virtual bool isReleaseClassified(Database::IdType releaseId) const = 0;
			virtual bool isArtistClassified(Database::IdType artistId) const = 0;

			virtual std::vector<Database::IdType> getSimilarTracksFromTrackList(Database::Session& session, Database::IdType tracklistId, std::size_t maxCount) = 0;
			virtual std::vector<Database::IdType> getSimilarTracks(Database::Session& session, const std::unordered_set<Database::IdType>& tracksId, std::size_t maxCount) const = 0;
			virtual std::vector<Database::IdType> getSimilarReleases(Database::IdType releaseId, std::size_t maxCount) const = 0;
			virtual std::vector<Database::IdType> getSimilarArtists(Database::IdType artistId, std::size_t maxCount) const = 0;
	};

} // ns Recommendation
