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
#include "database/ScanSettings.hpp"
#include "database/Track.hpp"
#include "recommendation/IEngine.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "resource/ImageResource.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "MediaPlayer.hpp"
#include "ReleaseListHelpers.hpp"
#include "TrackStringUtils.hpp"

using namespace Database;

namespace UserInterface {

Release::Release(Filters* filters)
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Release.template")}
, _filters(filters)
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	wApp->internalPathChanged().connect([=]()
	{
		refreshView();
	});

	filters->updated().connect([=]
	{
		refreshView();
	});

	refreshView();
}

void
Release::refreshView()
{
	if (!wApp->internalPathMatches("/release/"))
		return;

	clear();

	const auto releaseId {StringUtils::readAs<Database::IdType>(wApp->internalPathNextPart("/release/"))};
	if (!releaseId)
		return;

	const std::vector<Database::IdType> similarReleasesIds {ServiceProvider<Recommendation::IEngine>::get()->getSimilarReleases(LmsApp->getDbSession(), *releaseId, 5)};

    auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Release::pointer release {Database::Release::getById(LmsApp->getDbSession(), *releaseId)};
	if (!release)
		throw ReleaseNotFoundException {*releaseId};

	refreshCopyright(release);
	refreshSimilarReleases(similarReleasesIds);

	bindString("name", Wt::WString::fromUTF8(release->getName()), Wt::TextFormat::Plain);

	std::optional<int> year {release->getReleaseYear()};
	if (year)
	{
		setCondition("if-has-year", true);
		bindInt("year", *year);

		std::optional<int> originalYear {release->getReleaseYear(true)};
		if (originalYear && *originalYear != *year)
		{
			setCondition("if-has-orig-year", true);
			bindInt("orig-year", *originalYear);
		}
	}

	{
		std::vector<Wt::Dbo::ptr<Database::Artist>> artists;

		artists = release->getReleaseArtists();
		if (artists.empty())
			artists = release->getArtists();

		if (artists.size() > 1)
		{
			setCondition("if-has-artist", true);
			bindNew<Wt::WText>("artist", Wt::WString::tr("Lms.Explore.various-artists"));
		}
		else if (artists.size() == 1)
		{
			setCondition("if-has-artist", true);
			bindWidget("artist", LmsApplication::createArtistAnchor(artists.front()));
		}
	}

	{
		Wt::WImage* cover {bindNew<Wt::WImage>("cover", Wt::WLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 512)))};
		cover->setStyleClass("Lms-cover-large");
	}

	Wt::WContainerWidget* clusterContainers {bindNew<Wt::WContainerWidget>("clusters")};
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
		Wt::WText* playBtn {bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)};
		playBtn->clicked().connect([=]
		{
			releasesPlay.emit({*releaseId});
		});
	}

	{
		Wt::WText* addBtn {bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML)};
		addBtn->clicked().connect([=]
		{
			releasesAdd.emit({*releaseId});
		});
	}

	Wt::WContainerWidget* rootContainer {bindNew<Wt::WContainerWidget>("container")};

	const bool variousArtists {release->hasVariousArtists()};
	const auto totalDisc {release->getTotalDisc()};
	const bool isReleaseMultiDisc {totalDisc && *totalDisc > 1};

	// Expect to be call in asc order
	std::map<std::size_t, Wt::WContainerWidget*> trackContainers;
	auto getOrAddDiscContainer = [&](std::size_t discNumber, const std::string& discSubtitle) -> Wt::WContainerWidget*
	{
		{
			auto it = trackContainers.find(discNumber);
			if (it != std::cend(trackContainers))
				return it->second;
		}

		Wt::WTemplate* disc {rootContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.disc-entry"))};
		disc->addFunction("tr", &Wt::WTemplate::Functions::tr);

		if (discSubtitle.empty())
			disc->bindNew<Wt::WText>("disc-title", Wt::WString::tr("Lms.Explore.Release.disc").arg(discNumber));
		else
			disc->bindString("disc-title", Wt::WString::fromUTF8(discSubtitle), Wt::TextFormat::Plain);

		Wt::WContainerWidget* tracksContainer {disc->bindNew<Wt::WContainerWidget>("tracks")};
		trackContainers[discNumber] = tracksContainer;

		return tracksContainer;
	};

	const auto clusterIds {_filters->getClusterIds()};
	const auto tracks {release->getTracks(clusterIds)};

	for (const auto& track : tracks)
	{
		auto trackId {track.id()};

		const auto discNumber {track->getDiscNumber()};

		Wt::WContainerWidget* container {rootContainer};
		if (isReleaseMultiDisc && discNumber)
			container = getOrAddDiscContainer(*discNumber, track->getDiscSubtitle());

		Wt::WTemplate* entry {container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry"))};

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

		Wt::WText* playBtn {entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)};
		playBtn->clicked().connect([=]()
		{
			tracksPlay.emit({trackId});
		});

		Wt::WText* addBtn {entry->bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML)};
		addBtn->clicked().connect([=]()
		{
			tracksAdd.emit({trackId});
		});

		entry->bindString("duration", trackDurationToString(track->getDuration()), Wt::TextFormat::Plain);

		LmsApp->getMediaPlayer()->trackLoaded.connect(entry, [=] (Database::IdType loadedTrackId)
		{
			entry->bindString("is-playing", loadedTrackId == trackId ? "Lms-entry-playing" : "");
		});

		if (auto trackIdLoaded {LmsApp->getMediaPlayer()->getTrackLoaded()})
		{
			if (*trackIdLoaded == trackId)
				entry->bindString("is-playing", "Lms-entry-playing");
		}
	}
}

void
Release::refreshCopyright(const Database::Release::pointer& release)
{
	std::optional<std::string> copyright {release->getCopyright()};
	std::optional<std::string> copyrightURL {release->getCopyrightURL()};

	setCondition("if-has-copyright-or-copyright-url", copyright || copyrightURL);

	if (copyrightURL)
	{
		setCondition("if-has-copyright-url", true);

		Wt::WLink link {*copyrightURL};
		link.setTarget(Wt::LinkTarget::NewWindow);

		Wt::WAnchor* anchor {bindNew<Wt::WAnchor>("copyright-url", link)};
		anchor->setTextFormat(Wt::TextFormat::XHTML);
		anchor->setText(Wt::WString::tr("Lms.Explore.Release.template.link-btn"));
	}

	if (copyright)
	{
		setCondition("if-has-copyright", true);
		bindString("copyright", Wt::WString::fromUTF8(*copyright), Wt::TextFormat::Plain);
	}
}

void
Release::refreshSimilarReleases(const std::vector<Database::IdType>& similarReleasesId)
{
	auto* similarReleasesContainer {bindNew<Wt::WContainerWidget>("similar-releases")};

	for (Database::IdType id : similarReleasesId)
	{
		Database::Release::pointer similarRelease{Database::Release::getById(LmsApp->getDbSession(), id)};
		if (!similarRelease)
			continue;

		similarReleasesContainer->addWidget(ReleaseListHelpers::createEntrySmall(similarRelease));
	}
}

} // namespace UserInterface

