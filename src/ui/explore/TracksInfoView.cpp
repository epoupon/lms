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

#include "TracksInfoView.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLocalDateTime.h>

#include "database/Track.hpp"
#include "database/TrackList.hpp"

#include "utils/Utils.hpp"

#include "LmsApplication.hpp"

using namespace Database;

namespace {

using namespace UserInterface;

void addEntries(Wt::WContainerWidget *container, const std::vector<Track::pointer>& tracks)
{
	for (auto track : tracks)
	{
		Wt::WTemplate* entry = container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.TracksInfo.template.entry"));

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);
	}
}

}

namespace UserInterface {

TracksInfo::TracksInfo()
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.TracksInfo.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_mostPlayedContainer = bindNew<Wt::WContainerWidget>("most-played");
	_recentlyAddedContainer = bindNew<Wt::WContainerWidget>("recently-added");

	refreshMostPlayed();
	refreshRecentlyAdded();
}

void
TracksInfo::refreshRecentlyAdded()
{
	auto after = Wt::WLocalDateTime::currentServerDateTime().toUTC().addMonths(-1);

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
	auto tracks = Track::getLastAdded(LmsApp->getDboSession(), after, 5);

	_recentlyAddedContainer->clear();
	addEntries(_recentlyAddedContainer, tracks);
}

void
TracksInfo::refreshMostPlayed()
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
	auto tracks = LmsApp->getUser()->getPlayedTrackList()->getTopTracks(5);

	_mostPlayedContainer->clear();
	addEntries(_mostPlayedContainer, tracks);
}

} // namespace UserInterface

