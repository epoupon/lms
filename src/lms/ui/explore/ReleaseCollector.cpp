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

#include "ReleaseCollector.hpp"

#include <algorithm>

#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "services/database/TrackList.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Service.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	using namespace Database;

	RangeResults<ReleaseId>
	ReleaseCollector::get(Database::Range range)
	{
		Scrobbling::IScrobblingService& scrobbling {*Service<Scrobbling::IScrobblingService>::get()};

		range = getActualRange(range);

		RangeResults<ReleaseId> releases;
		switch (getMode())
		{
			case Mode::Random:
				releases = getRandomReleases(range);
				break;

			case Mode::Starred:
				releases = scrobbling.getStarredReleases(LmsApp->getUserId(), getFilters().getClusterIds(), range);
				break;

			case ReleaseCollector::Mode::RecentlyPlayed:
				releases = scrobbling.getRecentReleases(LmsApp->getUserId(), getFilters().getClusterIds(), range);
				break;

			case Mode::MostPlayed:
				releases = scrobbling.getTopReleases(LmsApp->getUserId(), getFilters().getClusterIds(), range);
				break;

			case Mode::RecentlyAdded:
			{
				Release::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setSortMethod(ReleaseSortMethod::LastWritten);
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					releases = Release::find(LmsApp->getDbSession(), params);
				}
				break;
			}

			case Mode::Search:
			{
				Release::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setKeywords(getSearchKeywords());
				params.setSortMethod(ReleaseSortMethod::Name);
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					releases = Release::find(LmsApp->getDbSession(), params);
				}
				break;
			}

			case Mode::All:
			{
				Release::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setSortMethod(ReleaseSortMethod::Name);
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					releases = Release::find(LmsApp->getDbSession(), params);
				}
				break;
			}
		}

		if (range.offset + range.size == getMaxCount())
			releases.moreResults = false;

		return releases;
	}

	RangeResults<ReleaseId>
	ReleaseCollector::getRandomReleases(Range range)
	{
		assert(getMode() == Mode::Random);

		if (!_randomReleases)
		{
			Release::FindParameters params;
			params.setClusters(getFilters().getClusterIds());
			params.setSortMethod(ReleaseSortMethod::Random);
			params.setRange({0, getMaxCount()});

			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};
				_randomReleases = Release::find(LmsApp->getDbSession(), params);
			}
		}

		return _randomReleases->getSubRange(range);
	}

} // ns UserInterface

