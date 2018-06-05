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
#include <Wt/WTemplate.h>
#include <Wt/WLineEdit.h>

#include "database/Artist.hpp"
#include "database/Setting.hpp"
#include "database/TrackStats.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"

using namespace Database;

namespace UserInterface {

Artists::Artists(Filters* filters)
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.Artists.template")),
  _filters(filters)
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_search = bindNew<Wt::WLineEdit>("search");
	_search->setPlaceholderText(Wt::WString::tr("Lms.Explore.search-placeholder"));
	_search->textInput().connect(this, &Artists::refresh);

	_artistsContainer = bindNew<Wt::WContainerWidget>("artists");
	_mostPlayedArtistsContainer = bindNew<Wt::WContainerWidget>("most-played-artists");

	_showMore = bindNew<Wt::WTemplate>("show-more", Wt::WString::tr("Lms.Explore.template.show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);

	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	refresh();
	refreshMostPlayed();

	filters->updated().connect(this, &Artists::refresh);
}

void
Artists::refreshMostPlayed()
{
	_mostPlayedArtistsContainer->clear();

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
	auto artists = TrackStats::getMostPlayedArtists(LmsApp->getDboSession(), LmsApp->getCurrentUser(), 5);

	for (auto artist : artists)
	{
		Wt::WTemplate* entry = _mostPlayedArtistsContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artists.template.most-played-entry"));

		entry->bindWidget("name", LmsApplication::createArtistAnchor(artist));
	}
}

void
Artists::refresh()
{
	_artistsContainer->clear();
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
			_artistsContainer->count(), 20, moreResults);

	for (auto artist : artists)
	{
		Wt::WTemplate* entry = _artistsContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artists.template.entry"));

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

