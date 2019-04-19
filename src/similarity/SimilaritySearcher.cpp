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
Database::SimilaritySettings::PreferredMethod getPreferredMethod(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction(session);
	return Database::SimilaritySettings::get(session)->getPreferredMethod();
}

std::vector<Database::IdType>
Searcher::getSimilarTracks(Wt::Dbo::Session& session, const std::set<Database::IdType>& trackIds, std::size_t maxCount)
{

	auto method = getPreferredMethod(session);
	auto somSearcher = _somAddon.getSearcher();

	if (method == Database::SimilaritySettings::PreferredMethod::Features
		|| (method == Database::SimilaritySettings::PreferredMethod::Auto
			&& somSearcher
			&& std::any_of(std::cbegin(trackIds), std::cend(trackIds), [&](Database::IdType trackId) { return somSearcher->isTrackClassified(trackId); } )))
	{
		return somSearcher->getSimilarTracks(trackIds, maxCount);
	}
	else
		return ClusterSearcher::getSimilarTracks(session, trackIds, maxCount);
}

std::vector<Database::IdType>
Searcher::getSimilarReleases(Wt::Dbo::Session& session, Database::IdType releaseId, std::size_t maxCount)
{
	auto method = getPreferredMethod(session);
	auto somSearcher = _somAddon.getSearcher();

	if (method == Database::SimilaritySettings::PreferredMethod::Features
		|| (method == Database::SimilaritySettings::PreferredMethod::Auto
			&& somSearcher
			&& somSearcher->isReleaseClassified(releaseId)))
	{
		return somSearcher->getSimilarReleases(releaseId, maxCount);
	}
	else
		return ClusterSearcher::getSimilarReleases(session, releaseId, maxCount);
}

std::vector<Database::IdType>
Searcher::getSimilarArtists(Wt::Dbo::Session& session, Database::IdType artistId, std::size_t maxCount)
{
	auto method = getPreferredMethod(session);
	auto somSearcher = _somAddon.getSearcher();

	if (method == Database::SimilaritySettings::PreferredMethod::Features
		|| (method == Database::SimilaritySettings::PreferredMethod::Auto
			&& somSearcher
			&& somSearcher->isArtistClassified(artistId)))
	{
		return somSearcher->getSimilarArtists(artistId, maxCount);
	}
	else
		return ClusterSearcher::getSimilarArtists(session, artistId, maxCount);
}

} // ns Similarity
