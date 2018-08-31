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

#include "ReleasesInfoView.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WLocalDateTime.h>
#include <Wt/WTemplate.h>

#include "database/Release.hpp"
#include "database/TrackList.hpp"

#include "resource/ImageResource.hpp"

#include "LmsApplication.hpp"

using namespace Database;

namespace {

using namespace UserInterface;

void addEntries(Wt::WContainerWidget* container, const std::vector<Release::pointer>& releases)
{
	for (auto release : releases)
	{
		Wt::WTemplate* entry = container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.ReleasesInfo.template.entry"));

		entry->setStyleClass("media");
		entry->bindWidget("release-name", LmsApplication::createReleaseAnchor(release));

		Wt::WAnchor* anchor = entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));
		auto cover = std::make_unique<Wt::WImage>();
		cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 48));
		cover->setWidth(48);
		anchor->setImage(std::move(cover));

		auto artists = release->getArtists();
		if (artists.size() > 1)
		{
			entry->setCondition("if-has-artist", true);
			entry->bindString("artist-name", Wt::WString::tr("Lms.Explore.various-artists"));
		}
		else if (artists.size() == 1)
		{
			entry->setCondition("if-has-artist", true);
			entry->bindWidget("artist-name", LmsApplication::createArtistAnchor(artists.front()));
		}
	}
}

} // namespace

namespace UserInterface {

ReleasesInfo::ReleasesInfo()
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.ReleasesInfo.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_mostPlayedContainer = bindNew<Wt::WContainerWidget>("most-played");
	_recentlyAddedContainer = bindNew<Wt::WContainerWidget>("recently-added");

	refreshRecentlyAdded();
	refreshMostPlayed();
}

void
ReleasesInfo::refreshRecentlyAdded()
{
	auto after = Wt::WLocalDateTime::currentServerDateTime().toUTC().addMonths(-1);

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	auto releases = Release::getLastAdded(LmsApp->getDboSession(), after, 5);

	_recentlyAddedContainer->clear();
	addEntries(_recentlyAddedContainer, releases);
}

void
ReleasesInfo::refreshMostPlayed()
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	auto releases = LmsApp->getUser()->getPlayedTrackList()->getTopReleases(5);

	_mostPlayedContainer->clear();
	addEntries(_mostPlayedContainer, releases);
}

} // namespace UserInterface

