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

#include "SimilarityClusterSearcher.hpp"

#include <random>
#include <chrono>

#include "database/Cluster.hpp"
#include "database/Track.hpp"
#include "utils/Utils.hpp"

namespace Similarity {

std::vector<Database::IdType>
ClusterSearcher::getSimilarTracks(Wt::Dbo::Session& session, const std::vector<Database::IdType>& tracksId, std::size_t maxCount)
{
	Wt::Dbo::Transaction transaction(session);

	std::vector<Database::IdType> clusterIds;
	for (auto trackId : tracksId)
	{
		auto track = Database::Track::getById(session, trackId);
		if (!track)
			continue;

		auto clusters = track->getClusters();
		if (clusters.empty())
			continue;

		for (const auto& cluster : clusters)
			clusterIds.push_back(cluster.id());
	}

	std::vector<Database::IdType> sortedClusterIds;
	uniqueAndSortedByOccurence(clusterIds.begin(), clusterIds.end(), std::back_inserter(sortedClusterIds));


#if 0
	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());


	std::set<Database::IdType> trackIds;
	{
		auto ids = tracklist->getTrackIds();
		trackIds = std::set<Database::IdType>(ids.begin(), ids.end());
	}

	// Get all the tracks of the tracklist, get the cluster that is mostly used
	// and reuse it to get the next track
	auto clusters = tracklist->getClusters();
	if (clusters.empty())
		return;

	for (auto cluster : clusters)
	{
		std::set<Database::IdType> clusterTrackIds = cluster->getTrackIds();

		std::set<Database::IdType> candidateTrackIds;
		std::set_difference(clusterTrackIds.begin(), clusterTrackIds.end(),
				trackIds.begin(), trackIds.end(),
				std::inserter(candidateTrackIds, candidateTrackIds.end()));

		if (candidateTrackIds.empty())
			continue;

		std::uniform_int_distribution<int> dist(0, candidateTrackIds.size() - 1);

		auto trackToAdd = Database::Track::getById(LmsApp->getDboSession(), *std::next(candidateTrackIds.begin(), dist(randGenerator)));
		enqueueTrack(trackToAdd);

		return;
	}

	LMS_LOG(UI, INFO) << "No more track to be added!";
#endif
	return {};
}

std::vector<Database::IdType>
ClusterSearcher::getSimilarReleases(Wt::Dbo::Session& session, Database::IdType releaseId, std::size_t maxCount)
{

	return {};
}

std::vector<Database::IdType>
ClusterSearcher::getSimilarArtists(Wt::Dbo::Session& session, Database::IdType artistId, std::size_t maxCount)
{
	return {};
}

} // namespace Similarity
