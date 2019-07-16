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

#include "SimilaritySearcher.hpp"

#include "features/SimilarityFeaturesScannerAddon.hpp"
#include "cluster/SimilarityClusterSearcher.hpp"

#include "database/SimilaritySettings.hpp"

namespace Similarity {

Searcher::Searcher(FeaturesScannerAddon& somAddon)
: _somAddon(somAddon)
{}

static
Database::SimilaritySettings::EngineType getEngineType(Database::Session& dbSession)
{
	auto transaction {dbSession.createSharedTransaction()};
	return Database::SimilaritySettings::get(dbSession)->getEngineType();
}

std::vector<Database::IdType>
Searcher::getSimilarTracks(Database::Session& dbSession, const std::set<Database::IdType>& trackIds, std::size_t maxCount)
{
	auto engineType {getEngineType(dbSession)};
	auto somSearcher {_somAddon.getSearcher()};

	if (engineType == Database::SimilaritySettings::EngineType::Features
		&& somSearcher
		&& std::any_of(std::cbegin(trackIds), std::cend(trackIds), [&](Database::IdType trackId) { return somSearcher->isTrackClassified(trackId); } ))
	{
		return somSearcher->getSimilarTracks(trackIds, maxCount);
	}
	else
		return ClusterSearcher::getSimilarTracks(dbSession, trackIds, maxCount);
}

std::vector<Database::IdType>
Searcher::getSimilarReleases(Database::Session& dbSession, Database::IdType releaseId, std::size_t maxCount)
{
	auto engineType {getEngineType(dbSession)};
	auto somSearcher {_somAddon.getSearcher()};

	if (engineType == Database::SimilaritySettings::EngineType::Features
		&& somSearcher
		&& somSearcher->isReleaseClassified(releaseId))
	{
		return somSearcher->getSimilarReleases(releaseId, maxCount);
	}
	else
		return ClusterSearcher::getSimilarReleases(dbSession, releaseId, maxCount);
}

std::vector<Database::IdType>
Searcher::getSimilarArtists(Database::Session& dbSession, Database::IdType artistId, std::size_t maxCount)
{
	auto engineType {getEngineType(dbSession)};
	auto somSearcher {_somAddon.getSearcher()};

	if (engineType == Database::SimilaritySettings::EngineType::Features
		&& somSearcher
		&& somSearcher->isArtistClassified(artistId))
	{
		return somSearcher->getSimilarArtists(artistId, maxCount);
	}
	else
		return ClusterSearcher::getSimilarArtists(dbSession, artistId, maxCount);
}

} // ns Similarity
