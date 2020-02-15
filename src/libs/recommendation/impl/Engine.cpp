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

#include "Engine.hpp"

//#include "features/SimilarityFeaturesScannerAddon.hpp"
//#include "cluster/SimilarityClusterSearcher.hpp"

#include "database/ScanSettings.hpp"
#include "database/TrackList.hpp"

namespace Recommendation {

std::unique_ptr<IEngine>
createEngine()
{
	return std::make_unique<Engine>();
}

void
Engine::clearClassifiers()
{
	_classifiers.clear();
}

void
Engine::addClassifier(std::unique_ptr<Classifier> classifier, unsigned priority)
{
	_classifiers.emplace(priority, std::move(classifier));
}

std::vector<Database::IdType>
Engine::getSimilarTracksFromTrackList(Database::Session& /*session*/, Database::IdType /*trackListId*/, std::size_t /*maxCount*/)
{
#if 0
	auto engineType {getEngineType(session)};
	auto somSearcher {_somAddon.getSearcher()};

	std::set<Database::IdType> trackIds;
	{
		auto transaction {session.createSharedTransaction()};
		Database::TrackList::pointer trackList {Database::TrackList::getById(session, trackListId)};
		if (trackList)
		{
			const std::vector<Database::IdType> orderedTrackIds {trackList->getTrackIds()};
			trackIds = std::set<Database::IdType> {std::cbegin(orderedTrackIds), std::cend(orderedTrackIds)};
		}
	}

	if (trackIds.empty())
		return {};

	if (engineType == Database::ScanSettings::SimilarityEngineType::Features
			&& somSearcher
			&& std::any_of(std::cbegin(trackIds), std::cend(trackIds), [&](Database::IdType trackId) { return somSearcher->isTrackClassified(trackId); } ))
	{
		return somSearcher->getSimilarTracks(trackIds, maxCount);
	}
	else
		return ClusterEngine::getSimilarTracksFromTrackList(session, trackListId, maxCount);
#endif
	return {};
}

std::vector<Database::IdType>
Engine::getSimilarTracks(Database::Session& /*dbSession*/, const std::unordered_set<Database::IdType>& /*trackIds*/, std::size_t /*maxCount*/)
{
#if 0
	auto engineType {getEngineType(dbSession)};
	auto somSearcher {_somAddon.getSearcher()};

	if (engineType == Database::ScanSettings::SimilarityEngineType::Features
		&& somSearcher
		&& std::any_of(std::cbegin(trackIds), std::cend(trackIds), [&](Database::IdType trackId) { return somSearcher->isTrackClassified(trackId); } ))
	{
		return somSearcher->getSimilarTracks(trackIds, maxCount);
	}
	else
		return ClusterEngine::getSimilarTracks(dbSession, trackIds, maxCount);
#endif
	return {};
}

std::vector<Database::IdType>
Engine::getSimilarReleases(Database::Session& /*dbSession*/, Database::IdType /*releaseId*/, std::size_t /*maxCount*/)
{
#if 0
	auto engineType {getEngineType(dbSession)};
	auto somSearcher {_somAddon.getSearcher()};

	if (engineType == Database::ScanSettings::SimilarityEngineType::Features
		&& somSearcher
		&& somSearcher->isReleaseClassified(releaseId))
	{
		return somSearcher->getSimilarReleases(releaseId, maxCount);
	}
	else
		return ClusterEngine::getSimilarReleases(dbSession, releaseId, maxCount);
#endif
	return {};
}

std::vector<Database::IdType>
Engine::getSimilarArtists(Database::Session& /*dbSession*/, Database::IdType /*artistId*/, std::size_t /*maxCount*/)
{
#if 0
	auto engineType {getEngineType(dbSession)};
	auto somSearcher {_somAddon.getSearcher()};

	if (engineType == Database::ScanSettings::SimilarityEngineType::Features
		&& somSearcher
		&& somSearcher->isArtistClassified(artistId))
	{
		return somSearcher->getSimilarArtists(artistId, maxCount);
	}
	else
		return ClusterEngine::getSimilarArtists(dbSession, artistId, maxCount);
#endif
	return {};
}

} // ns Similarity
