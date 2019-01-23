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

namespace Similarity {

Searcher::Searcher(SOMScannerAddon& somAddon)
: _somAddon(somAddon)
{}

std::vector<Database::IdType>
Searcher::getSimilarTracks(const std::vector<Database::IdType>& tracksId, std::size_t maxCount)
{
	auto somSearcher = _somAddon.getSearcher();
	if (!somSearcher)
		return {};

	return somSearcher->getSimilarTracks(tracksId, maxCount);
}

std::vector<Database::IdType>
Searcher::getSimilarReleases(Wt::Dbo::Session& session, Database::IdType releaseId, std::size_t maxCount)
{
	auto somSearcher = _somAddon.getSearcher();
	if (!somSearcher)
		return {};

	return somSearcher->getSimilarReleases(session, releaseId, maxCount);
}

std::vector<Database::IdType>
Searcher::getSimilarArtists(Wt::Dbo::Session& session, Database::IdType artistId, std::size_t maxCount)
{
	auto somSearcher = _somAddon.getSearcher();
	if (!somSearcher)
		return {};

	return somSearcher->getSimilarArtists(session, artistId, maxCount);
}

} // ns Similarity
