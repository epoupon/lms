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

#include <functional>
#include <string_view>
#include <vector>

#include "database/Types.hpp"
#include "recommendation/IRecommendation.hpp"
#include "utils/EnumSet.hpp"

namespace Database
{
	class Session;
}

namespace Recommendation
{

	class IClassifier : public IRecommendation
	{
		public:
			virtual ~IClassifier() = default;

			virtual std::string_view getName() const = 0;

			struct Progress
			{
				std::size_t totalElems {};
				std::size_t	processedElems {};
			};
			using ProgressCallback = std::function<void(const Progress&)>;
			virtual bool load(Database::Session& session, bool forceReload, const ProgressCallback& progressCallback) = 0;
			virtual void requestCancelLoad() = 0;

			template <typename IdType>
			using ResultContainer = std::vector<IdType>;

			virtual ResultContainer<Database::TrackId> getSimilarTracksFromTrackList(Database::Session& session, Database::TrackListId tracklistId, std::size_t maxCount) const = 0;
			virtual ResultContainer<Database::TrackId> getSimilarTracks(Database::Session& session, const std::vector<Database::TrackId>& tracksId, std::size_t maxCount) const = 0;
			virtual ResultContainer<Database::ReleaseId> getSimilarReleases(Database::Session& session, Database::ReleaseId releaseId, std::size_t maxCount) const = 0;
			virtual ResultContainer<Database::ArtistId> getSimilarArtists(Database::Session& session,
					Database::ArtistId artistId,
					EnumSet<Database::TrackArtistLinkType> linkTypes, std::size_t maxCount) const = 0;
	};

} // ns Recommendation
