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

#include "ReleasesView.hpp"

#include <Wt/WApplication.h>
#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WText.h>
#include <Wt/WTemplate.h>

#include "database/Release.hpp"
#include "database/Setting.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "resource/ImageResource.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"

using namespace Database;

namespace {

const std::string releasesClusterTypesSetting = "releases_cluster_types";
const std::vector<std::string> defaultReleasesClusterTypes =
{
	"GENRE",
	"ALBUMGROUPING",
};

std::vector<ClusterType::pointer> getReleasesClusterTypes(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction(session);

	std::vector<ClusterType::pointer> res;
	for (auto clusterTypeName : splitString(Setting::getString(session, releasesClusterTypesSetting), " "))
	{
		auto clusterType = ClusterType::getByName(session, clusterTypeName);
		if (clusterType)
			res.push_back(clusterType);
	}

	return res;
}

} // namespace

namespace UserInterface {

Releases::Releases(Filters* filters)
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.Releases.template")),
_filters(filters)
{
	if (!Setting::exists(LmsApp->getDboSession(), releasesClusterTypesSetting))
		Setting::setString(LmsApp->getDboSession(), releasesClusterTypesSetting, joinStrings(defaultReleasesClusterTypes, " "));

	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_search = bindNew<Wt::WLineEdit>("search");
	_search->setPlaceholderText(Wt::WString::tr("Lms.Explore.search-placeholder"));
	_search->textInput().connect(this, &Releases::refresh);

	_releasesContainer = bindNew<Wt::WContainerWidget>("releases");

	_showMore = bindNew<Wt::WTemplate>("show-more", Wt::WString::tr("Lms.Explore.show-more"));
	_showMore->addFunction("tr", &Wt::WTemplate::Functions::tr);
	_showMore->clicked().connect(std::bind([=]
	{
		addSome();
	}));

	refresh();

	filters->updated().connect(this, &Releases::refresh);
}

void
Releases::refresh()
{
	_releasesContainer->clear();
	addSome();
}

void
Releases::addSome()
{
	auto searchKeywords = splitString(_search->text().toUTF8(), " ");

	auto clusterIds = _filters->getClusterIds();

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	bool moreResults;
	auto releases = Release::getByFilter(LmsApp->getDboSession(), clusterIds, searchKeywords, _releasesContainer->count(), 20, moreResults);

	for (auto release : releases)
	{
		auto releaseId = release.id();

		Wt::WTemplate* entry = _releasesContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Releases.template.entry"));
		entry->addFunction("tr", Wt::WTemplate::Functions::tr);

		Wt::WAnchor* anchor = entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));
		auto cover = std::make_unique<Wt::WImage>();
		cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 128));
		anchor->setImage(std::move(cover));

		entry->bindWidget("release-name", LmsApplication::createReleaseAnchor(release));

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

		Wt::WContainerWidget* clusterContainers = entry->bindNew<Wt::WContainerWidget>("clusters");
		{
			auto clusterTypes = getReleasesClusterTypes(LmsApp->getDboSession());
			auto clusterGroups = release->getClusterGroups(clusterTypes, 1);

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

		Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.Releases.play"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect(std::bind([=]
		{
			releasePlay.emit(releaseId);
		}));

		Wt::WText* addBtn = entry->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.Releases.add"), Wt::TextFormat::XHTML);
		addBtn->clicked().connect(std::bind([=]
		{
			releaseAdd.emit(releaseId);
		}));
	}

	_showMore->setHidden(!moreResults);
}

} // namespace UserInterface

