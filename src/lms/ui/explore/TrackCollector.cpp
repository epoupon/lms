/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "TrackCollector.hpp"

#include <algorithm>

#include "lmscore/database/Session.hpp"
#include "lmscore/database/Track.hpp"
#include "lmscore/database/TrackList.hpp"
#include "lmscore/database/User.hpp"
#include "scrobbling/IScrobbling.hpp"
#include "utils/Service.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	using namespace Database;

	std::vector<Track::pointer>
	TrackCollector::get(std::optional<Database::Range> range, bool& moreResults)
	{
		Scrobbling::IScrobbling& scrobbling {*Service<Scrobbling::IScrobbling>::get()};
		range = getActualRange(range);

		std::vector<Track::pointer> tracks;
		switch (getMode())
		{
			case Mode::Random:
				tracks = getRandomTracks(range, moreResults);
				break;

			case Mode::Starred:
				tracks = Track::getStarred(LmsApp->getDbSession(), LmsApp->getUser(), getFilters().getClusterIds(), range, moreResults);
				break;

			case TrackCollector::Mode::RecentlyPlayed:
				for (const TrackId trackId : scrobbling.getRecentTracks(LmsApp->getUserId(), getFilters().getClusterIds(), range, moreResults))
				{
					if (const Track::pointer track {Track::getById(LmsApp->getDbSession(), trackId)})
						tracks.push_back(track);
				}
				break;

			case Mode::MostPlayed:
				for (const TrackId trackId : scrobbling.getTopTracks(LmsApp->getUserId(), getFilters().getClusterIds(), range, moreResults))
				{
					if (const Track::pointer track {Track::getById(LmsApp->getDbSession(), trackId)})
						tracks.push_back(track);
				}
				break;

			case Mode::RecentlyAdded:
				tracks = Track::getLastWritten(LmsApp->getDbSession(), std::nullopt, getFilters().getClusterIds(), range, moreResults);
				break;

			case Mode::Search:
				tracks = Track::getByFilter(LmsApp->getDbSession(), getFilters().getClusterIds(), getSearchKeywords(), range, moreResults);
				break;

			case Mode::All:
				tracks = Track::getByFilter(LmsApp->getDbSession(), getFilters().getClusterIds(), {}, range, moreResults);
				break;
		}

		if (range && getMaxCount() && (range->offset + range->limit == *getMaxCount()))
			moreResults = false;

		return tracks;
	}

	std::vector<Database::TrackId>
	TrackCollector::getAll()
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		bool moreResults;
		const auto tracks {get(std::nullopt, moreResults)};

		std::vector<TrackId> res;
		res.reserve(tracks.size());
		std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const Track::pointer& track) { return track->getId(); });

		return res;
	}

	std::vector<Database::Track::pointer>
	TrackCollector::getRandomTracks(std::optional<Range> range, bool& moreResults)
	{
		std::vector<Track::pointer> tracks;

		assert(getMode() == Mode::Random);

		if (_randomTracks.empty())
			_randomTracks = Track::getAllIdsRandom(LmsApp->getDbSession(), getFilters().getClusterIds(), getMaxCount());

		{
			auto itBegin {std::cbegin(_randomTracks) + std::min(range ? range->offset : 0, _randomTracks.size())};
			auto itEnd {std::cbegin(_randomTracks) + std::min(range ? range->offset + range->limit : _randomTracks.size(), _randomTracks.size())};

			for (auto it {itBegin}; it != itEnd; ++it)
			{
				Track::pointer track {Track::getById(LmsApp->getDbSession(), *it)};
				if (track)
					tracks.push_back(track);
			}

			moreResults = (itEnd != std::cend(_randomTracks));
		}

		return tracks;
	}

} // ns UserInterface

