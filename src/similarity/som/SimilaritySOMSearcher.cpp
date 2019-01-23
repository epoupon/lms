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

#include "SimilaritySOMSearcher.hpp"

#include <random>

#include "database/Artist.hpp"
#include "database/SimilaritySettings.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

namespace Similarity {

SOMSearcher::SOMSearcher(ConstructionParams params)
: _network(std::move(params.network)),
_normalizer(std::move(params.normalizer)),
_tracksMap(std::move(params.tracksMap)),
_trackIdsCoords(std::move(params.trackIdsCoords))
{
}

std::vector<Database::IdType>
SOMSearcher::getSimilarTracks(const std::vector<Database::IdType>& tracksIds, std::size_t maxCount)
{
	std::vector<Database::IdType> res;

	auto bestCoords = getBestMatchingCoords(tracksIds);
	if (!bestCoords)
		return res;

	auto tracks = _tracksMap[*bestCoords];

	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	std::shuffle(tracks.begin(), tracks.end(), randGenerator);

	if (tracks.size() > maxCount)
		tracks.resize(maxCount);

	return tracks;
}

std::vector<Database::IdType>
SOMSearcher::getSimilarReleases(Wt::Dbo::Session& session, Database::IdType releaseId, std::size_t maxCount)
{
	std::vector<Database::IdType> res;

	Wt::Dbo::Transaction transaction(session);

	auto release = Database::Release::getById(session, releaseId);
	if (!release)
		return res;

	auto tracks = release->getTracks();

	std::vector<Database::IdType> tracksIds;
	for (auto track : tracks)
		tracksIds.push_back(track.id());

	auto matchingCoords = getMatchingCoords(tracksIds);
	if (matchingCoords.empty())
		return res;

	auto releases = getReleases(session, matchingCoords);
	uniqueAndSortedByOccurence(releases.begin(), releases.end(), std::back_inserter(res));

	res.erase(std::remove_if(res.begin(), res.end(), [&](auto releaseId) { return releaseId == release.id(); }), res.end());

	if (res.size() > maxCount)
		res.resize(maxCount);

	LMS_LOG(SIMILARITY, DEBUG) << "*** SIMILARITY RESULT *** :";
	for (auto id : res)
		LMS_LOG(SIMILARITY, DEBUG) << id;

	return res;
}

std::vector<Database::IdType>
SOMSearcher::getSimilarArtists(Wt::Dbo::Session& session, Database::IdType artistId, std::size_t maxCount)
{
	std::vector<Database::IdType> res;

	Wt::Dbo::Transaction transaction(session);

	auto artist = Database::Artist::getById(session, artistId);
	if (!artist)
		return res;

	auto tracks = artist->getTracks();

	std::vector<Database::IdType> tracksIds;
	for (auto track : tracks)
		tracksIds.push_back(track.id());

	auto matchingCoords = getMatchingCoords(tracksIds);
	if (matchingCoords.empty())
		return res;

	auto artists = getArtists(session, matchingCoords);
	uniqueAndSortedByOccurence(artists.begin(), artists.end(), std::back_inserter(res));

	res.erase(std::remove_if(res.begin(), res.end(), [&](auto artistId) { return artistId == artist.id(); }), res.end());

	if (res.size() > maxCount)
		res.resize(maxCount);

	LMS_LOG(SIMILARITY, DEBUG) << "*** SIMILARITY RESULT *** :";
	for (auto id : res)
		LMS_LOG(SIMILARITY, DEBUG) << id;

	return res;
}

void
SOMSearcher::dump(Wt::Dbo::Session& session, std::ostream& os) const
{
	os << "Number of tracks classified: " << _trackIdsCoords.size() << std::endl;
	os << "Network size: " << _network.getWidth() << " * " << _network.getHeight() << std::endl;

	Wt::Dbo::Transaction transaction(session);

	for (std::size_t y = 0; y < _network.getHeight(); ++y)
	{
		for (std::size_t x = 0; x < _network.getWidth(); ++x)
		{
			const auto& trackIds = _tracksMap[{x, y}];

			for (auto trackId : trackIds)
			{
				auto track = Database::Track::getById(session, trackId);
				if (!track)
					continue;

				os << "{";
				if (track->getArtist())
					os << track->getArtist()->getName() << " ";
				if (track->getRelease())
					os << track->getRelease()->getName();
				os << "} ";
			}

			os << "; ";
		}
		os << std::endl;
	}
}

boost::optional<SOM::Coords>
SOMSearcher::getBestMatchingCoords(const std::vector<Database::IdType>& tracksIds) const
{
	if (tracksIds.empty())
		return boost::none;

	std::map<SOM::Coords, std::size_t /*count*/> coordsCount;

	for (auto trackId : tracksIds)
	{
		auto it = _trackIdsCoords.find(trackId);
		if (it == _trackIdsCoords.end())
			continue;

		if (coordsCount.find(it->second) == coordsCount.end())
			coordsCount[it->second] = 0;

		coordsCount[it->second]++;
	}

	if (coordsCount.empty())
		return boost::none;

	auto bestCoords = std::max_element(std::begin(coordsCount), std::end(coordsCount),
			[](const auto& a, const auto& b)
			{
				return a.second < b.second;
			});

	return bestCoords->first;
}

std::vector<SOM::Coords>
SOMSearcher::getMatchingCoords(const std::vector<Database::IdType>& tracksIds) const
{
	std::vector<SOM::Coords> res;

	if (tracksIds.empty())
		return res;

	for (auto trackId : tracksIds)
	{
		auto it = _trackIdsCoords.find(trackId);
		if (it == _trackIdsCoords.end())
			continue;

		res.push_back(it->second);
	}

	return res;
}

std::vector<Database::IdType>
SOMSearcher::getReleases(Wt::Dbo::Session& session, const std::vector<SOM::Coords>& coords) const
{
	std::vector<Database::IdType> res;
	for (const auto& c : coords)
	{
		for (auto trackId : _tracksMap[c])
		{
			auto track = Database::Track::getById(session, trackId);
			if (!track || !track->getRelease())
				continue;

			res.emplace_back(track->getRelease().id());
		}
	}

	return res;
}

std::vector<Database::IdType>
SOMSearcher::getArtists(Wt::Dbo::Session& session, const std::vector<SOM::Coords>& coords) const
{
	std::vector<Database::IdType> res;
	for (const auto& c : coords)
	{
		for (auto trackId : _tracksMap[c])
		{
			auto track = Database::Track::getById(session, trackId);
			if (!track || !track->getArtist())
				continue;

			res.emplace_back(track->getArtist().id());
		}
	}

	return res;
}

} // ns Similarity
