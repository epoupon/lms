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

#include "database/Release.hpp"
#include "database/User.hpp"
#include "database/TrackList.hpp"
#include "scrobbling/IScrobbling.hpp"
#include "utils/Service.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	using namespace Database;

	std::vector<Release::pointer>
	ReleaseCollector::get(std::optional<Database::Range> range, bool& moreResults)
	{
		range = getActualRange(range);

		std::vector<Release::pointer> releases;
		switch (getMode())
		{
			case Mode::Random:
				releases = getRandomReleases(range, moreResults);
				break;

			case Mode::Starred:
				releases = Release::getStarred(LmsApp->getDbSession(), LmsApp->getUser(), getFilters().getClusterIds(), range, moreResults);
				break;

			case ReleaseCollector::Mode::RecentlyPlayed:
				releases = Service<Scrobbling::IScrobbling>::get()->getRecentReleases(LmsApp->getDbSession(), LmsApp->getUser(), getFilters().getClusterIds(), range, moreResults);
				break;

			case Mode::MostPlayed:
				releases = Service<Scrobbling::IScrobbling>::get()->getTopReleases(LmsApp->getDbSession(), LmsApp->getUser(), getFilters().getClusterIds(), range, moreResults);
				break;

			case Mode::RecentlyAdded:
				releases = Release::getLastWritten(LmsApp->getDbSession(), std::nullopt, getFilters().getClusterIds(), range, moreResults);
				break;

			case Mode::Search:
				releases = Release::getByFilter(LmsApp->getDbSession(), getFilters().getClusterIds(), getSearchKeywords(), range, moreResults);
				break;

			case Mode::All:
				releases = Release::getByFilter(LmsApp->getDbSession(), getFilters().getClusterIds(), {}, range, moreResults);
				break;
		}

		if (range && getMaxCount() && (range->offset + range->limit == *getMaxCount()))
			moreResults = false;

		return releases;
	}

	std::vector<Database::IdType>
	ReleaseCollector::getAll()
	{
		bool moreResults;
		const auto releases {get(std::nullopt, moreResults)};

		std::vector<IdType> res;
		res.reserve(releases.size());
		std::transform(std::cbegin(releases), std::cend(releases), std::back_inserter(res), [](const Release::pointer& release) { return release.id(); });

		return res;
	}

	std::vector<Database::Release::pointer>
	ReleaseCollector::getRandomReleases(std::optional<Range> range, bool& moreResults)
	{
		std::vector<Release::pointer> releases;

		assert(getMode() == Mode::Random);

		if (_randomReleases.empty())
			_randomReleases = Release::getAllIdsRandom(LmsApp->getDbSession(), getFilters().getClusterIds(), getMaxCount());

		{
			auto itBegin {std::cbegin(_randomReleases) + std::min(range ? range->offset : 0, _randomReleases.size())};
			auto itEnd {std::cbegin(_randomReleases) + std::min(range ? range->offset + range->limit : _randomReleases.size(), _randomReleases.size())};

			for (auto it {itBegin}; it != itEnd; ++it)
			{
				Release::pointer release {Release::getById(LmsApp->getDbSession(), *it)};
				if (release)
					releases.push_back(release);
			}

			moreResults = (itEnd != std::cend(_randomReleases));
		}

		return releases;
	}

} // ns UserInterface

