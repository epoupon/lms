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

#include "ArtistsInfoView.hpp"

#include <Wt/WLocalDateTime.h>

#include "database/Artist.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"
#include "ArtistLink.hpp"
#include "LmsApplication.hpp"

using namespace Database;

namespace UserInterface {

ArtistsInfo::ArtistsInfo()
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.ArtistsInfo.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_mostPlayedContainer = bindNew<Wt::WContainerWidget>("most-played");
	_recentlyAddedContainer = bindNew<Wt::WContainerWidget>("recently-added");

	LmsApp->getEvents().dbScanned.connect([=]
	{
		refreshRecentlyAdded();
	});

	LmsApp->getEvents().trackLoaded.connect([=]
	{
		refreshMostPlayed();
	});

	refreshMostPlayed();
	refreshRecentlyAdded();
}

void
ArtistsInfo::refreshRecentlyAdded()
{
	auto after = Wt::WLocalDateTime::currentServerDateTime().toUTC().addMonths(-1);

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};
	const std::vector<Database::Artist::pointer> artists {Artist::getLastAdded(LmsApp->getDbSession(), after, 5)};

	_recentlyAddedContainer->clear();
	for (const Database::Artist::pointer& artist : artists)
		_recentlyAddedContainer->addNew<ArtistLink>(artist);
}

void
ArtistsInfo::refreshMostPlayed()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const std::vector<Database::Artist::pointer> artists {LmsApp->getUser()->getPlayedTrackList(LmsApp->getDbSession())->getTopArtists(5)};

	_mostPlayedContainer->clear();
	for (const Database::Artist::pointer& artist : artists)
		_mostPlayedContainer->addNew<ArtistLink>(artist);
}

} // namespace UserInterface

