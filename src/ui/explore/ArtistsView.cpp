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

#include "database/DbArtist.hpp"
#include "database/Setting.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"

using namespace Database;

namespace {

const std::string artistsClusterTypesSetting = "artists_cluster_types";
const std::vector<std::string> defaultArtistsClusterTypes =
{
	"ALBUMGROUPING",
	"GENRE",
	"ALBUMMOOD",
};

std::vector<ClusterType::pointer> getArtistsClusterTypes(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction(session);

	std::vector<ClusterType::pointer> res;
	for (auto clusterTypeName : splitString(Setting::getString(session, artistsClusterTypesSetting), " "))
	{
		auto clusterType = ClusterType::getByName(session, clusterTypeName);
		if (clusterType)
			res.push_back(clusterType);
	}

	return res;
}

}

namespace UserInterface {

Artists::Artists(Filters* filters)
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.Artists.template")),
  _filters(filters)
{
	if (!Setting::exists(LmsApp->getDboSession(), artistsClusterTypesSetting))
		Setting::setString(LmsApp->getDboSession(), artistsClusterTypesSetting, joinStrings(defaultArtistsClusterTypes, " "));

	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_search = bindNew<Wt::WLineEdit>("search");
	_search->setPlaceholderText(Wt::WString::tr("Lms.Explore.search-placeholder"));
	_search->textInput().connect(this, &Artists::refresh);

	_artistsContainer = bindNew<Wt::WContainerWidget>("artists");

	_showMore = bindNew<Wt::WTemplate>("show-more", Wt::WString::tr("Lms.Explore.template.show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);

	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	refresh();

	filters->updated().connect(this, &Artists::refresh);
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
			auto clusterTypes = getArtistsClusterTypes(LmsApp->getDboSession());
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

