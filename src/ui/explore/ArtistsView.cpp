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

#include "ArtistsView.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WLineEdit.h>
#include <Wt/WLocalDateTime.h>
#include <Wt/WServer.h>
#include <Wt/WTemplate.h>

#include "database/Artist.hpp"
#include "database/Setting.hpp"
#include "database/TrackStats.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"

using namespace Database;

namespace {

using namespace UserInterface;

void addCompactEntries(Wt::WContainerWidget *container, const std::vector<Artist::pointer>& artists)
{
	for (auto artist : artists)
	{
		Wt::WTemplate* entry = container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artists.template.compact-entry"));

		entry->bindWidget("name", LmsApplication::createArtistAnchor(artist));
	}
}

}

namespace UserInterface {

Artists::Artists(Filters* filters)
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.Artists.template")),
  _filters(filters)
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_search = bindNew<Wt::WLineEdit>("search");
	_search->setPlaceholderText(Wt::WString::tr("Lms.Explore.search-placeholder"));
	_search->textInput().connect(this, &Artists::refresh);

	_container = bindNew<Wt::WContainerWidget>("artists");
	_mostPlayedContainer = bindNew<Wt::WContainerWidget>("most-played-artists");
	_recentlyAddedContainer = bindNew<Wt::WContainerWidget>("recently-added-artists");

	_showMore = bindNew<Wt::WTemplate>("show-more", Wt::WString::tr("Lms.Explore.template.show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);

	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	refresh();
	refreshMostPlayed();
	refreshRecentlyAdded();

	filters->updated().connect(this, &Artists::refresh);

	std::string sessionId = LmsApp->sessionId();
	LmsApp->getMediaScanner().scanComplete().connect([=] (Scanner::MediaScanner::Stats stats)
	{
		if (stats.nbChanges() > 0)
		{
			Wt::WServer::instance()->post(sessionId, [=]
			{
				refreshRecentlyAdded();
				LmsApp->triggerUpdate();
			});
		}
	});
}

void
Artists::refreshRecentlyAdded()
{
	auto after = Wt::WLocalDateTime::currentServerDateTime().toUTC().addMonths(-1);

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
	auto artists = Artist::getLastAdded(LmsApp->getDboSession(), after, 5);

	_recentlyAddedContainer->clear();
	addCompactEntries(_recentlyAddedContainer, artists);
}

void
Artists::refreshMostPlayed()
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
	auto artists = TrackStats::getMostPlayedArtists(LmsApp->getDboSession(), LmsApp->getCurrentUser(), 5);

	_mostPlayedContainer->clear();
	addCompactEntries(_mostPlayedContainer, artists);
}

void
Artists::refresh()
{
	_container->clear();
	addSome();
}

void
Artists::addSome()
{
	auto searchKeywords = splitString(_search->text().toUTF8(), " ");

	auto clusterIds = _filters->getClusterIds();

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	bool moreResults;
	auto artists = Artist::getByFilter(LmsApp->getDboSession(),
			clusterIds,
			searchKeywords,
			_container->count(), 20, moreResults);

	for (auto artist : artists)
	{
		Wt::WTemplate* entry = _container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artists.template.entry"));

		entry->bindInt("nb-release", artist->getReleases(clusterIds).size());
		entry->bindWidget("name", LmsApplication::createArtistAnchor(artist));


		Wt::WContainerWidget* clusterContainers = entry->bindNew<Wt::WContainerWidget>("clusters");
		{
			auto clusterTypes = ScanSettings::get(LmsApp->getDboSession())->getClusterTypes();
			auto clusterGroups = artist->getClusterGroups(clusterTypes, 1);

			for (auto clusters : clusterGroups)
			{
				for (auto cluster : clusters)
				{
					auto clusterId = cluster.id();
					auto entry = clusterContainers->addWidget(LmsApp->createCluster(cluster));
					entry->clicked().connect([=]
					{
						_filters->add(clusterId);
					});
				}
			}
		}
	}

	_showMore->setHidden(!moreResults);
}

} // namespace UserInterface

