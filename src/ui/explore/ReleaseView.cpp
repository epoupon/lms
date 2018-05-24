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

#include "ReleaseView.hpp"

#include <Wt/WApplication.h>
#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "database/Release.hpp"
#include "database/Track.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "resource/ImageResource.hpp"

#include "LmsApplication.hpp"
#include "Filters.hpp"

using namespace Database;

namespace UserInterface {

Release::Release(Filters* filters)
: _filters(filters)
{
	wApp->internalPathChanged().connect(std::bind([=]
	{
		refresh();
	}));

	refresh();

	filters->updated().connect(std::bind([=] {
		refresh();
	}));
}

void
Release::refresh()
{
	if (!wApp->internalPathMatches("/release/"))
		return;

	clear();

	auto releaseId = readAs<Database::IdType>(wApp->internalPathNextPart("/release/"));
	if (!releaseId)
		return;

        Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	auto release = Database::Release::getById(LmsApp->getDboSession(), *releaseId);
	if (!release)
	{
		LmsApp->goHome();
		return;
	}

	Wt::WTemplate* t = addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template"));
	t->addFunction("tr", &Wt::WTemplate::Functions::tr);

	t->bindString("name", Wt::WString::fromUTF8(release->getName()), Wt::TextFormat::Plain);

	boost::optional<int> year = release->getReleaseYear();
	if (year)
	{
		t->setCondition("if-has-year", true);
		t->bindInt("year", *year);

		boost::optional<int> originalYear = release->getReleaseYear(true);
		if (originalYear && *originalYear != *year)
		{
			t->setCondition("if-has-orig-year", true);
			t->bindInt("orig-year", *originalYear);
		}
	}

	{
		auto artists = release->getArtists();
		if (artists.size() > 1)
		{
			t->setCondition("if-has-artist", true);
			t->bindString("artist-name", Wt::WString::tr("Lms.Explore.various-artists"));
		}
		else if (artists.size() == 1)
		{
			t->setCondition("if-has-artist", true);
			t->bindWidget("artist-name", LmsApplication::createArtistAnchor(artists.front()));
		}
	}

	t->bindNew<Wt::WImage>("cover", Wt::WLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 512)));

	Wt::WContainerWidget* clusterContainers = t->bindNew<Wt::WContainerWidget>("clusters");
	{
		auto clusterTypes = ScanSettings::get(LmsApp->getDboSession())->getClusterTypes();
		auto clusterGroups = release->getClusterGroups(clusterTypes, 3);

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

	{
		Wt::WText* playBtn = t->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.Release.play"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect(std::bind([=]
		{
			releasePlay.emit(*releaseId);
		}));
	}

	{
		Wt::WText* addBtn =  t->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.Release.add"), Wt::TextFormat::XHTML);
		addBtn->clicked().connect(std::bind([=]
		{
			releaseAdd.emit(*releaseId);
		}));
	}

	Wt::WContainerWidget* tracksContainer = t->bindNew<Wt::WContainerWidget>("tracks");

	auto clusterIds = _filters->getClusterIds();
	auto tracks = release->getTracks(clusterIds);

	bool variousArtists = release->hasVariousArtists();

	for (auto track : tracks)
	{
		auto trackId = track.id();

		Wt::WTemplate* entry = tracksContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry"));
		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		if (variousArtists && track->getArtist())
		{
			entry->setCondition("if-has-artist", true);
			entry->bindWidget("artist-name", LmsApplication::createArtistAnchor(track->getArtist()));
		}

		auto trackNumber = track->getTrackNumber();
		if (trackNumber)
		{
			entry->setCondition("if-has-track-number", true);
			entry->bindInt("track-number", *trackNumber);
		}

		auto discNumber = track->getDiscNumber();
		auto totalDiscNumber = track->getTotalDiscNumber();
		if (discNumber && totalDiscNumber && *totalDiscNumber > 1)
		{
			entry->setCondition("if-has-disc-number", true);
			entry->bindInt("disc-number", *discNumber);
		}

		Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.Release.play"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect(std::bind([=]
		{
			trackPlay.emit(trackId);
		}));

		Wt::WText* addBtn = entry->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.Release.add"), Wt::TextFormat::XHTML);
		addBtn->clicked().connect(std::bind([=]
		{
			trackAdd.emit(trackId);
		}));
	}
}

} // namespace UserInterface

