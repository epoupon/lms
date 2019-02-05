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

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "utils/Utils.hpp"

namespace Similarity {
namespace ClusterSearcher {

std::vector<Database::IdType>
getSimilarTracks(Wt::Dbo::Session& session, const std::set<Database::IdType>& trackIds, std::size_t maxCount)
{
	std::vector<Database::IdType> res;

	Wt::Dbo::Transaction transaction(session);

	std::vector<Database::IdType> clusterIds;
	for (auto trackId : trackIds)
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

	for (auto clusterId : clusterIds)
	{
		auto cluster = Database::Cluster::getById(session, clusterId);
		if (!cluster)
			continue;

		std::set<Database::IdType> clusterTrackIds = cluster->getTrackIds();

		std::set<Database::IdType> candidateTrackIds;
		std::set_difference(clusterTrackIds.begin(), clusterTrackIds.end(),
				trackIds.begin(), trackIds.end(),
				std::inserter(candidateTrackIds, candidateTrackIds.end()));

		if (candidateTrackIds.empty())
			continue;

		for (auto trackId : candidateTrackIds)
		{
			if (res.size() >= maxCount)
				break;

			res.push_back(trackId);
		}

		if (res.size() >= maxCount)
			break;
	}

	return res;
}

std::vector<Database::IdType>
getSimilarReleases(Wt::Dbo::Session& session, Database::IdType releaseId, std::size_t maxCount)
{
	std::vector<Database::IdType> res;

	Wt::Dbo::Transaction transaction(session);

	auto release = Database::Release::getById(session, releaseId);
	if (!release)
		return res;

	auto releaseTracks = release->getTracks();
	std::set<Database::IdType> releaseTrackIds;

	for (const auto& releaseTrack : releaseTracks)
		releaseTrackIds.insert(releaseTrack.id());

	auto trackIds = getSimilarTracks(session, releaseTrackIds, maxCount * 5);

	for (auto trackId : trackIds)
	{
		auto track = Database::Track::getById(session, trackId);
		if (!track)
			continue;

		auto trackRelease = track->getRelease();
		if (!trackRelease || trackRelease.id() == releaseId)
			continue;

		if (std::find(res.begin(), res.end(), trackRelease.id()) != res.end())
			continue;

		res.push_back(trackRelease.id());

		if (res.size() == maxCount)
			break;
	}

	return res;
}

std::vector<Database::IdType>
getSimilarArtists(Wt::Dbo::Session& session, Database::IdType artistId, std::size_t maxCount)
{
	std::vector<Database::IdType> res;

	Wt::Dbo::Transaction transaction(session);

	auto artist = Database::Artist::getById(session, artistId);
	if (!artist)
		return res;

	auto artistTracks = artist->getTracks();
	std::set<Database::IdType> artistTrackIds;

	for (const auto& artistTrack : artistTracks)
		artistTrackIds.insert(artistTrack.id());

	auto trackIds = getSimilarTracks(session, artistTrackIds, maxCount * 5);

	for (auto trackId : trackIds)
	{
		auto track = Database::Track::getById(session, trackId);
		if (!track)
			continue;

		auto trackArtist = track->getArtist();
		if (!trackArtist || trackArtist.id() == artistId)
			continue;

		if (std::find(res.begin(), res.end(), trackArtist.id()) != res.end())
			continue;

		res.push_back(trackArtist.id());

		if (res.size() == maxCount)
			break;
	}

	return res;
}

} // namespace ClusterSearcher
} // namespace Similarity
