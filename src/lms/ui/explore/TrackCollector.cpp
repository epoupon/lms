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

#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Service.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	using namespace Database;

	RangeResults<TrackId>
	TrackCollector::get(Range range)
	{
		Scrobbling::IScrobblingService& scrobbling {*Service<Scrobbling::IScrobblingService>::get()};
		range = getActualRange(range);

		RangeResults<TrackId> tracks;
		switch (getMode())
		{
			case Mode::Random:
				tracks = getRandomTracks(range);
				break;

			case Mode::Starred:
				tracks = scrobbling.getStarredTracks(LmsApp->getUserId(), getFilters().getClusterIds(), range);
				break;

			case TrackCollector::Mode::RecentlyPlayed:
				tracks = scrobbling.getRecentTracks(LmsApp->getUserId(), getFilters().getClusterIds(), range);
				break;

			case Mode::MostPlayed:
				tracks = scrobbling.getTopTracks(LmsApp->getUserId(), getFilters().getClusterIds(), range);
				break;

			case Mode::RecentlyAdded:
			{
				Track::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setSortMethod(TrackSortMethod::LastWritten);
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					tracks = Track::find(LmsApp->getDbSession(), params);
				}
				break;
			}

			case Mode::Search:
			{
				Track::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setKeywords(getSearchKeywords());
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					tracks = Track::find(LmsApp->getDbSession(), params);
				}
				break;
			}

			case Mode::All:
			{
				Track::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					tracks = Track::find(LmsApp->getDbSession(), params);
				}
				break;
			}
		}

		if (range.offset + range.size == getMaxCount())
			tracks.moreResults = false;

		return tracks;
	}

	RangeResults<TrackId>
	TrackCollector::getRandomTracks(Range range)
	{
		assert(getMode() == Mode::Random);

		if (!_randomTracks)
		{
			Track::FindParameters params;
			params.setClusters(getFilters().getClusterIds());
			params.setSortMethod(TrackSortMethod::Random);
			params.setRange({0, getMaxCount()});

			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};
				_randomTracks = Track::find(LmsApp->getDbSession(), params);
			}
		}

		return _randomTracks->getSubRange(range);
	}

} // ns UserInterface

