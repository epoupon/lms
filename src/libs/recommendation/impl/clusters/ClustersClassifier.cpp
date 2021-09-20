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

std::unique_ptr<IClassifier> createClustersClassifier()
{
	return std::make_unique<ClusterClassifier>();
}

IClassifier::ResultContainer<Database::TrackId>
ClusterClassifier::getSimilarTracks(Database::Session& dbSession, const std::vector<Database::TrackId>& trackIds, std::size_t maxCount) const
{
	ResultContainer<Database::TrackId> res;

	auto transaction {dbSession.createSharedTransaction()};

	const auto tracks {Database::Track::getSimilarTracks(dbSession, trackIds, 0, maxCount)};
	std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const auto& track) { return track->getId(); });

	return res;
}

IClassifier::ResultContainer<Database::TrackId>
ClusterClassifier::getSimilarTracksFromTrackList(Database::Session& session, Database::TrackListId tracklistId, std::size_t maxCount) const
{
	ResultContainer<Database::TrackId> res;

	auto transaction {session.createSharedTransaction()};

	const Database::TrackList::pointer trackList {Database::TrackList::getById(session, tracklistId)};
	if (!trackList)
		return res;

	const auto tracks {trackList->getSimilarTracks(0, maxCount)};
	std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const auto& track) { return track->getId(); });

	return res;
}

IClassifier::ResultContainer<Database::ReleaseId>
ClusterClassifier::getSimilarReleases(Database::Session& dbSession, Database::ReleaseId releaseId, std::size_t maxCount) const
{
	ResultContainer<Database::ReleaseId> res;

	auto transaction {dbSession.createSharedTransaction()};

	auto release {Database::Release::getById(dbSession, releaseId)};
	if (!release)
		return res;

	const auto releases {release->getSimilarReleases(0, maxCount)};
	std::transform(std::cbegin(releases), std::cend(releases), std::back_inserter(res), [](const auto& release) { return release->getId(); });

	return res;
}

IClassifier::ResultContainer<Database::ArtistId>
ClusterClassifier::getSimilarArtists(Database::Session& dbSession,
		Database::ArtistId artistId,
		EnumSet<Database::TrackArtistLinkType> artistLinkTypes,
		std::size_t maxCount) const
{
	ResultContainer<Database::ArtistId> res;

	auto transaction {dbSession.createSharedTransaction()};

	auto artist {Database::Artist::getById(dbSession, artistId)};
	if (!artist)
		return res;

	const auto artists {artist->getSimilarArtists(artistLinkTypes, Database::Range {0, maxCount})};
	std::transform(std::cbegin(artists), std::cend(artists), std::back_inserter(res), [](const auto& artist) { return artist->getId(); });

	return res;
}

} // namespace Recommendation
