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

#include "IClassifier.hpp"

namespace Recommendation
{

	class ClusterClassifier : public IClassifier
	{
		public:
			ClusterClassifier() = default;
			ClusterClassifier(const ClusterClassifier&) = delete;
			ClusterClassifier(ClusterClassifier&&) = delete;
			ClusterClassifier& operator=(const ClusterClassifier&) = delete;
			ClusterClassifier& operator=(ClusterClassifier&&) = delete;

		private:

			std::string_view getName() const override { return "Clusters"; }

			bool load(Database::Session&, bool, const ProgressCallback&) override { return true; }
			void requestCancelLoad() override {}

			ResultContainer<Database::TrackId> getSimilarTracksFromTrackList(Database::Session& session, Database::TrackListId tracklistId, std::size_t maxCount) const override;
			ResultContainer<Database::TrackId> getSimilarTracks(Database::Session& session, const std::vector<Database::TrackId>& tracksId, std::size_t maxCount) const override;
			ResultContainer<Database::ReleaseId> getSimilarReleases(Database::Session& session, Database::ReleaseId releaseId, std::size_t maxCount) const override;
			ResultContainer<Database::ArtistId> getSimilarArtists(Database::Session& session,
					Database::ArtistId artistId,
					EnumSet<Database::TrackArtistLinkType> linkTypes,
					std::size_t maxCount) const override;
};

} // namespace Recommendation

