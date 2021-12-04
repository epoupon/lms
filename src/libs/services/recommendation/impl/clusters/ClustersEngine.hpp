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

#include "IEngine.hpp"

namespace Recommendation
{

	class ClusterEngine : public IEngine
	{
		public:
			ClusterEngine(Database::Db& db) : _db {db} {}

			ClusterEngine(const ClusterEngine&) = delete;
			ClusterEngine(ClusterEngine&&) = delete;
			ClusterEngine& operator=(const ClusterEngine&) = delete;
			ClusterEngine& operator=(ClusterEngine&&) = delete;

		private:
			void load(bool, const ProgressCallback&) override {}
			void requestCancelLoad() override {}

			TrackContainer		findSimilarTracksFromTrackList(Database::TrackListId tracklistId, std::size_t maxCount) const override;
			TrackContainer		findSimilarTracks(const std::vector<Database::TrackId>& tracksId, std::size_t maxCount) const override;
			ReleaseContainer	getSimilarReleases(Database::ReleaseId releaseId, std::size_t maxCount) const override;
			ArtistContainer		getSimilarArtists(Database::ArtistId artistId, EnumSet<Database::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

			Database::Db& _db;
	};

} // namespace Recommendation

