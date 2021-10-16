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
#include <memory>

#include "lmscore/database/Types.hpp"
#include "utils/EnumSet.hpp"

namespace Database
{
	class Db;
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
			virtual void cancelLoad() = 0;  // wait for cancel done
			virtual void requestCancelLoad() = 0;

			template <typename IdType>
			using ResultContainer = std::vector<IdType>;

			using ArtistContainer = ResultContainer<Database::ArtistId>;
			using ReleaseContainer = ResultContainer<Database::ReleaseId>;
			using TrackContainer = ResultContainer<Database::TrackId>;

			virtual TrackContainer getSimilarTracksFromTrackList(Database::TrackListId tracklistId, std::size_t maxCount) const = 0;
			virtual TrackContainer getSimilarTracks(const std::vector<Database::TrackId>& tracksId, std::size_t maxCount) const = 0;
			virtual ReleaseContainer getSimilarReleases(Database::ReleaseId releaseId, std::size_t maxCount) const = 0;
			virtual ArtistContainer getSimilarArtists(Database::ArtistId artistId, EnumSet<Database::TrackArtistLinkType> linkTypes, std::size_t maxCount) const = 0;
	};

	std::unique_ptr<IEngine> createEngine(Database::Db& db);

} // ns Recommendation

