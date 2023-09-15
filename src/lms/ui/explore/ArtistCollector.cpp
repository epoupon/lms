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

#include "services/database/Artist.hpp"
#include "services/database/Session.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Service.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	using namespace Database;

	RangeResults<ArtistId>
	ArtistCollector::get(Database::Range range)
	{
		Scrobbling::IScrobblingService& scrobbling {*Service<Scrobbling::IScrobblingService>::get()};

		range = getActualRange(range);

		RangeResults<ArtistId> artists;
		switch (getMode())
		{
			case Mode::Random:
				artists = getRandomArtists(range);
				break;

			case Mode::Starred:
				artists = scrobbling.getStarredArtists(LmsApp->getUserId(), getFilters().getClusterIds(), _linkType, ArtistSortMethod::StarredDateDesc, range);
				break;

			case Mode::RecentlyPlayed:
				artists = scrobbling.getRecentArtists(LmsApp->getUserId(), getFilters().getClusterIds(), _linkType, range);
				break;

			case Mode::MostPlayed:
				artists = scrobbling.getTopArtists(LmsApp->getUserId(), getFilters().getClusterIds(), _linkType, range);
				break;

			case Mode::RecentlyAdded:
			{
				Artist::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setLinkType(_linkType);
				params.setSortMethod(ArtistSortMethod::LastWritten);
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					artists = Artist::find(LmsApp->getDbSession(), params);
				}
				break;
			}

			case Mode::Search:
			{
				Artist::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setKeywords(getSearchKeywords());
				params.setLinkType(_linkType);
				params.setSortMethod(ArtistSortMethod::BySortName);
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					artists = Artist::find(LmsApp->getDbSession(), params);
				}
				break;
			}

			case Mode::All:
			{
				Artist::FindParameters params;
				params.setClusters(getFilters().getClusterIds());
				params.setLinkType(_linkType);
				params.setSortMethod(ArtistSortMethod::BySortName);
				params.setRange(range);

				{
					auto transaction {LmsApp->getDbSession().createSharedTransaction()};
					artists = Artist::find(LmsApp->getDbSession(), params);
				}
				break;
			}
		}

		if (range.offset + range.size == getMaxCount())
			artists.moreResults = false;

		return artists;
	}

	RangeResults<Database::ArtistId>
	ArtistCollector::getRandomArtists(Range range)
	{
		assert(getMode() == Mode::Random);

		if (!_randomArtists)
		{
			Artist::FindParameters params;
			params.setClusters(getFilters().getClusterIds());
			params.setLinkType(_linkType);
			params.setSortMethod(ArtistSortMethod::Random);
			params.setRange(Range {0, getMaxCount()});

			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};
				_randomArtists = Artist::find(LmsApp->getDbSession(), params);
			}
		}

		return _randomArtists->getSubRange(range);
	}
} // ns UserInterface

