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

#include "ArtistCollector.hpp"

#include "database/Artist.hpp"
#include "database/User.hpp"
#include "database/TrackList.hpp"
#include "scrobbling/IScrobbling.hpp"
#include "utils/Service.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	using namespace Database;

	std::vector<Database::ObjectPtr<Database::Artist>>
	ArtistCollector::get(std::optional<Database::Range> range, bool& moreResults)
	{
		range = getActualRange(range);

		std::vector<Artist::pointer> artists;
		switch (getMode())
		{
			case Mode::Random:
				artists = getRandomArtists(range, moreResults);
				break;

			case Mode::Starred:
				artists = Artist::getStarred(LmsApp->getDbSession(),
								LmsApp->getUser(),
								getFilters().getClusterIds(),
								_linkType,
								Artist::SortMethod::BySortName,
								range, moreResults);
				break;

			case Mode::RecentlyPlayed:
				artists = Service<Scrobbling::IScrobbling>::get()->getRecentArtists(LmsApp->getDbSession(), LmsApp->getUser(),
								getFilters().getClusterIds(),
								_linkType,
								range, moreResults);
				break;

			case Mode::MostPlayed:
				artists = Service<Scrobbling::IScrobbling>::get()->getTopArtists(LmsApp->getDbSession(), LmsApp->getUser(),
								getFilters().getClusterIds(),
								_linkType,
								range, moreResults);
				break;

			case Mode::RecentlyAdded:
				artists = Artist::getLastWritten(LmsApp->getDbSession(),
								std::nullopt, // after
								getFilters().getClusterIds(),
								_linkType,
								range, moreResults);
				break;

			case Mode::Search:
				artists = Database::Artist::getByFilter(LmsApp->getDbSession(),
								getFilters().getClusterIds(),
								getSearchKeywords(),
								std::nullopt, // no link
								Database::Artist::SortMethod::BySortName,
								range, moreResults);
				break;

			case Mode::All:
				artists = Artist::getByFilter(LmsApp->getDbSession(),
						getFilters().getClusterIds(),
						{},
						_linkType,
						Artist::SortMethod::BySortName,
						range, moreResults);
				break;
		}

		if (range && getMaxCount() && (range->offset + range->limit == *getMaxCount()))
			moreResults = false;

		return artists;
	}

	std::vector<Database::Artist::pointer>
	ArtistCollector::getRandomArtists(std::optional<Range> range, bool& moreResults)
	{
		std::vector<Artist::pointer> artists;

		assert(getMode() == Mode::Random);

		if (_randomArtists.empty())
			_randomArtists = Artist::getAllIdsRandom(LmsApp->getDbSession(), getFilters().getClusterIds(), _linkType, getMaxCount());

		{
			auto itBegin {std::cbegin(_randomArtists) + std::min(range ? range->offset : 0, _randomArtists.size())};
			auto itEnd {std::cbegin(_randomArtists) + std::min(range ? range->offset + range->limit : _randomArtists.size(), _randomArtists.size())};

			for (auto it {itBegin}; it != itEnd; ++it)
			{
				Artist::pointer artist {Artist::getById(LmsApp->getDbSession(), *it)};
				if (artist)
					artists.push_back(artist);
			}

			moreResults = (itEnd != std::cend(_randomArtists));
		}

		return artists;
	}

} // ns UserInterface

