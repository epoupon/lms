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

#include "recommendation/IClassifier.hpp"


namespace Recommendation
{

	class ClusterClassifier : public IClassifier
	{
		public:
			ClusterClassifier(Database::Session& session);
			ClusterClassifier(const ClusterClassifier&) = delete;
			ClusterClassifier(ClusterClassifier&&) = delete;
			ClusterClassifier& operator=(const ClusterClassifier&) = delete;
			ClusterClassifier& operator=(ClusterClassifier&&) = delete;

		private:

			bool isTrackClassified(Database::IdType trackId) const override;
			bool isReleaseClassified(Database::IdType releaseId) const override;
			bool isArtistClassified(Database::IdType artistId) const override;

			std::vector<Database::IdType> getSimilarTracksFromTrackList(Database::Session& session, Database::IdType tracklistId, std::size_t maxCount) const override;
			std::vector<Database::IdType> getSimilarTracks(Database::Session& session, const std::unordered_set<Database::IdType>& tracksId, std::size_t maxCount) const override;
			std::vector<Database::IdType> getSimilarReleases(Database::Session& session, Database::IdType releaseId, std::size_t maxCount) const override;
			std::vector<Database::IdType> getSimilarArtists(Database::Session& session, Database::IdType artistId, std::size_t maxCount) const override;

			void classify(Database::Session& session);

			std::unordered_set<Database::IdType>	_classifiedArtists;
			std::unordered_set<Database::IdType>	_classifiedReleases;
			std::unordered_set<Database::IdType>	_classifiedTracks;
};

} // namespace Recommendation

