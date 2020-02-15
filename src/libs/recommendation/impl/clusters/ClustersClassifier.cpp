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

#include "ClustersClassifier.hpp"

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"

namespace Recommendation {
	
std::unique_ptr<IClassifier> createClustersClassifier(Database::Session& session)
{
	return std::make_unique<ClusterClassifier>(session);
}

ClusterClassifier::ClusterClassifier(Database::Session& session)
{
	classify(session);
}

void
ClusterClassifier::classify(Database::Session& session)
{
	auto transaction {session.createSharedTransaction()};

	{
		std::vector<Database::IdType> trackIds {Database::Track::getAllIdsWithClusters(session)};
		_classifiedTracks = std::unordered_set<Database::IdType>(std::cbegin(trackIds), std::cend(trackIds));
	}

	{
		std::vector<Database::IdType> releaseIds {Database::Release::getAllIdsWithClusters(session)};
		_classifiedReleases = std::unordered_set<Database::IdType>(std::cbegin(releaseIds), std::cend(releaseIds));
	}

	{
		std::vector<Database::IdType> artistIds {Database::Artist::getAllIdsWithClusters(session)};
		_classifiedArtists = std::unordered_set<Database::IdType>(std::cbegin(artistIds), std::cend(artistIds));
	}
}

bool
ClusterClassifier::isTrackClassified(Database::IdType trackId) const
{
	return _classifiedTracks.find(trackId) != std::cend(_classifiedTracks);
}

bool
ClusterClassifier::isReleaseClassified(Database::IdType releaseId) const
{
	return _classifiedReleases.find(releaseId) != std::cend(_classifiedReleases);
}

bool
ClusterClassifier::isArtistClassified(Database::IdType artistId) const
{
	return _classifiedArtists.find(artistId) != std::cend(_classifiedArtists);
}


std::vector<Database::IdType>
ClusterClassifier::getSimilarTracks(Database::Session& dbSession, const std::unordered_set<Database::IdType>& trackIds, std::size_t maxCount) const
{
	auto transaction {dbSession.createSharedTransaction()};

	auto tracks {Database::Track::getSimilarTracks(dbSession, trackIds, 0, maxCount)};
	std::vector<Database::IdType> res;
	res.reserve(tracks.size());

	std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const auto& track) { return track.id(); });
	return res;
}

std::vector<Database::IdType>
ClusterClassifier::getSimilarTracksFromTrackList(Database::Session& session, Database::IdType tracklistId, std::size_t maxCount) const
{
	std::vector<Database::IdType> res;

	auto transaction {session.createSharedTransaction()};

	const Database::TrackList::pointer trackList {Database::TrackList::getById(session, tracklistId)};
	if (!trackList)
		return res;

	const std::vector<Database::Track::pointer> tracks {trackList->getSimilarTracks(0, maxCount)};
	res.reserve(tracks.size());
	std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res),
			[](const Database::Track::pointer& track) { return track.id(); });

	return res;
}

std::vector<Database::IdType>
ClusterClassifier::getSimilarReleases(Database::Session& dbSession, Database::IdType releaseId, std::size_t maxCount) const
{
	std::vector<Database::IdType> res;

	auto transaction {dbSession.createSharedTransaction()};

	auto release {Database::Release::getById(dbSession, releaseId)};
	if (!release)
		return res;

	const auto releases {release->getSimilarReleases(0, maxCount)};
	res.reserve(releases.size());
	std::transform(std::cbegin(releases), std::cend(releases), std::back_inserter(res), [](const auto& release) { return release.id(); });

	return res;
}

std::vector<Database::IdType>
ClusterClassifier::getSimilarArtists(Database::Session& dbSession, Database::IdType artistId, std::size_t maxCount) const
{
	std::vector<Database::IdType> res;

	auto transaction {dbSession.createSharedTransaction()};

	auto artist {Database::Artist::getById(dbSession, artistId)};
	if (!artist)
		return res;

	const auto artists {artist->getSimilarArtists(0, maxCount)};
	res.reserve(artists.size());
	std::transform(std::cbegin(artists), std::cend(artists), std::back_inserter(res), [](const auto& artist) { return artist.id(); });

	return res;
}

} // namespace Recommendation
