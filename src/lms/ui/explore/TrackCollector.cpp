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

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"
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
		range = getActualRange(range);

		std::vector<Track::pointer> releases;
		switch (getMode())
		{
			case Mode::Random:
				releases = getRandomTracks(range, moreResults);
				break;

			case Mode::Starred:
				releases = Track::getStarred(LmsApp->getDbSession(), LmsApp->getUser(), getFilters().getClusterIds(), range, moreResults);
				break;

			case TrackCollector::Mode::RecentlyPlayed:
				releases = Service<Scrobbling::IScrobbling>::get()->getRecentTracks(LmsApp->getDbSession(), LmsApp->getUser(), getFilters().getClusterIds(), range, moreResults);
				break;

			case Mode::MostPlayed:
				releases = Service<Scrobbling::IScrobbling>::get()->getTopTracks(LmsApp->getDbSession(), LmsApp->getUser(), getFilters().getClusterIds(), range, moreResults);
				break;

			case Mode::RecentlyAdded:
				releases = Track::getLastWritten(LmsApp->getDbSession(), std::nullopt, getFilters().getClusterIds(), range, moreResults);
				break;

			case Mode::Search:
				releases = Track::getByFilter(LmsApp->getDbSession(), getFilters().getClusterIds(), getSearchKeywords(), range, moreResults);
				break;

			case Mode::All:
				releases = Track::getByFilter(LmsApp->getDbSession(), getFilters().getClusterIds(), {}, range, moreResults);
				break;
		}

		if (range && getMaxCount() && (range->offset + range->limit == *getMaxCount()))
			moreResults = false;

		return releases;
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
		std::vector<Track::pointer> releases;

		assert(getMode() == Mode::Random);

		if (_randomTracks.empty())
			_randomTracks = Track::getAllIdsRandom(LmsApp->getDbSession(), getFilters().getClusterIds(), getMaxCount());

		{
			auto itBegin {std::cbegin(_randomTracks) + std::min(range ? range->offset : 0, _randomTracks.size())};
			auto itEnd {std::cbegin(_randomTracks) + std::min(range ? range->offset + range->limit : _randomTracks.size(), _randomTracks.size())};

			for (auto it {itBegin}; it != itEnd; ++it)
			{
				Track::pointer release {Track::getById(LmsApp->getDbSession(), *it)};
				if (release)
					releases.push_back(release);
			}

			moreResults = (itEnd != std::cend(_randomTracks));
		}

		return releases;
	}

} // ns UserInterface

