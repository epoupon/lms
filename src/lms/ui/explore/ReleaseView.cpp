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

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WImage.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "database/Release.hpp"
#include "database/Track.hpp"

#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "resource/ImageResource.hpp"

#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "TrackStringUtils.hpp"

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

	filters->updated().connect([=]
	{
		refresh();
	});
}

void
Release::refresh()
{
	if (!wApp->internalPathMatches("/release/"))
		return;

	clear();
	auto releaseId {StringUtils::readAs<Database::IdType>(wApp->internalPathNextPart("/release/"))};
	if (!releaseId)
		return;

        auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Release::pointer release {Database::Release::getById(LmsApp->getDbSession(), *releaseId)};
	if (!release)
		throw ReleaseNotFoundException {*releaseId};

	Wt::WTemplate* t {addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template"))};
	t->addFunction("tr", &Wt::WTemplate::Functions::tr);

	t->bindString("name", Wt::WString::fromUTF8(release->getName()), Wt::TextFormat::Plain);

	std::optional<int> year {release->getReleaseYear()};
	if (year)
	{
		t->setCondition("if-has-year", true);
		t->bindInt("year", *year);

		std::optional<int> originalYear {release->getReleaseYear(true)};
		if (originalYear && *originalYear != *year)
		{
			t->setCondition("if-has-orig-year", true);
			t->bindInt("orig-year", *originalYear);
		}
	}

	{
		std::vector<Wt::Dbo::ptr<Database::Artist>> artists;

		artists = release->getReleaseArtists();
		if (artists.empty())
			artists = release->getArtists();

		if (artists.size() > 1)
		{
			t->setCondition("if-has-artist", true);
			t->bindNew<Wt::WText>("artist", Wt::WString::tr("Lms.Explore.various-artists"));
		}
		else if (artists.size() == 1)
		{
			t->setCondition("if-has-artist", true);
			t->bindWidget("artist", LmsApplication::createArtistAnchor(artists.front()));
		}
	}

	t->bindNew<Wt::WImage>("cover", Wt::WLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 512)));

	Wt::WContainerWidget* clusterContainers {t->bindNew<Wt::WContainerWidget>("clusters")};
	{
		const auto clusterTypes {ScanSettings::get(LmsApp->getDbSession())->getClusterTypes()};
		const auto clusterGroups {release->getClusterGroups(clusterTypes, 3)};

		for (const auto& clusters : clusterGroups)
		{
			for (const auto& cluster : clusters)
			{
				auto clusterId {cluster.id()};
				auto entry {clusterContainers->addWidget(LmsApp->createCluster(cluster))};
				entry->clicked().connect([=]
				{
					_filters->add(clusterId);
				});
			}
		}
	}

	{
		Wt::WText* playBtn {t->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)};
		playBtn->clicked().connect([=]
		{
			releasesPlay.emit({*releaseId});
		});
	}

	{
		Wt::WText* addBtn {t->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML)};
		addBtn->clicked().connect([=]
		{
			releasesAdd.emit({*releaseId});
		});
	}

	Wt::WContainerWidget* tracksContainer {t->bindNew<Wt::WContainerWidget>("tracks")};

	auto clusterIds {_filters->getClusterIds()};
	auto tracks {release->getTracks(clusterIds)};

	bool variousArtists {release->hasVariousArtists()};

	for (const auto& track : tracks)
	{
		auto trackId {track.id()};

		Wt::WTemplate* entry {tracksContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry"))};
		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		auto artists {track->getArtists()};
		if (variousArtists && !artists.empty())
		{
			entry->setCondition("if-has-artists", true);

			Wt::WContainerWidget* artistsContainer {entry->bindNew<Wt::WContainerWidget>("artists")};
			for (const auto& artist : artists)
			{
				Wt::WTemplate *t {artistsContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry-artist"))};
				t->bindWidget("artist", LmsApplication::createArtistAnchor(artist));
			}
		}

		auto trackNumber {track->getTrackNumber()};
		if (trackNumber)
		{
			entry->setCondition("if-has-track-number", true);
			entry->bindInt("track-number", *trackNumber);
		}

		auto discNumber {track->getDiscNumber()};
		auto totalDiscNumber {release->getTotalDiscNumber()};
		if (discNumber && totalDiscNumber && *totalDiscNumber > 1)
		{
			entry->setCondition("if-has-disc-number", true);
			entry->bindInt("disc-number", *discNumber);
		}

		Wt::WText* playBtn {entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)};
		playBtn->clicked().connect(std::bind([=]
		{
			tracksPlay.emit({trackId});
		}));

		Wt::WText* addBtn {entry->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML)};
		addBtn->clicked().connect(std::bind([=]
		{
			tracksAdd.emit({trackId});
		}));

		entry->bindString("duration", trackDurationToString(track->getDuration()), Wt::TextFormat::Plain);

		LmsApp->getEvents().trackLoaded.connect(entry, [=] (Database::IdType loadedTrackId, bool /*play*/)
		{
			entry->toggleStyleClass("Lms-explore-release-entry-playing", loadedTrackId == trackId);
		});

		if (LmsApp->getEvents().lastLoadedTrackId && *LmsApp->getEvents().lastLoadedTrackId == trackId)
			entry->addStyleClass("Lms-explore-release-entry-playing");
	}
}

} // namespace UserInterface

