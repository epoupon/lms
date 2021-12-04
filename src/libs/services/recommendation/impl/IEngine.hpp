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

#include <memory>
#include "services/database/Types.hpp"
#include "services/database/TrackListId.hpp"
#include "services/recommendation/Types.hpp"
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

			virtual void load(bool forceReload, const ProgressCallback& progressCallback = {}) = 0;
			virtual void requestCancelLoad() = 0;

			virtual TrackContainer findSimilarTracksFromTrackList(Database::TrackListId tracklistId, std::size_t maxCount) const = 0;
			virtual TrackContainer findSimilarTracks(const std::vector<Database::TrackId>& tracksId, std::size_t maxCount) const = 0;
			virtual ReleaseContainer getSimilarReleases(Database::ReleaseId releaseId, std::size_t maxCount) const = 0;
			virtual ArtistContainer getSimilarArtists(Database::ArtistId artistId, EnumSet<Database::TrackArtistLinkType> linkTypes, std::size_t maxCount) const = 0;
	};

	std::unique_ptr<IEngine> createEngine(Database::Db& db);

} // ns Recommendation

