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

#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "recommendation/IEngine.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "resource/DownloadResource.hpp"
#include "resource/CoverResource.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "MediaPlayer.hpp"
#include "ReleaseListHelpers.hpp"
#include "ReleasePopup.hpp"
#include "TrackPopup.hpp"
#include "TrackStringUtils.hpp"

using namespace Database;

namespace UserInterface {

Release::Release(Filters* filters)
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Release.template")}
, _filters {filters}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	wApp->internalPathChanged().connect(this, [this]
	{
		refreshView();
	});

	filters->updated().connect([this]
	{
		refreshView();
	});

	refreshView();
}

static
std::optional<ReleaseId>
extractReleaseIdFromInternalPath()
{
	if (wApp->internalPathMatches("/release/mbid/"))
	{
		const auto mbid {UUID::fromString(wApp->internalPathNextPart("/release/mbid/"))};
		if (mbid)
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};
			if (const Database::Release::pointer release {Database::Release::getByMBID(LmsApp->getDbSession(), *mbid)})
				return release->getId();
		}

		return std::nullopt;
	}

	return StringUtils::readAs<Database::ReleaseId::ValueType>(wApp->internalPathNextPart("/release/"));
}


void
Release::refreshView()
{
	if (!wApp->internalPathMatches("/release/"))
		return;

	clear();

	const auto releaseId {extractReleaseIdFromInternalPath()};
	if (!releaseId)
		throw ReleaseNotFoundException {};

	auto similarReleasesIds {Service<Recommendation::IEngine>::get()->getSimilarReleases(*releaseId, 6)};

    auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Release::pointer release {Database::Release::getById(LmsApp->getDbSession(), *releaseId)};
	if (!release)
		throw ReleaseNotFoundException {};

	refreshCopyright(release);
	refreshLinks(release);
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

	refreshReleaseArtists(release);

	{
		Wt::WImage* cover {bindNew<Wt::WImage>("cover", Wt::WLink(LmsApp->getCoverResource()->getReleaseUrl(release->getId(), CoverResource::Size::Large)))};
		cover->setStyleClass("Lms-cover-large");
		cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)");
	}

	Wt::WContainerWidget* clusterContainers {bindNew<Wt::WContainerWidget>("clusters")};
	{
		const auto clusterTypes {ScanSettings::get(LmsApp->getDbSession())->getClusterTypes()};
		const auto clusterGroups {release->getClusterGroups(clusterTypes, 3)};

		for (const auto& clusters : clusterGroups)
		{
			for (const auto& cluster : clusters)
			{
				const ClusterId clusterId {cluster->getId()};
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
			releasesAction.emit(PlayQueueAction::Play, {*releaseId});
		});
	}

	{
		Wt::WText* moreBtn {bindNew<Wt::WText>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML)};
		moreBtn->clicked().connect([=]
		{
			displayReleasePopupMenu(*moreBtn, *releaseId, releasesAction);
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

		Wt::WTemplate* disc {rootContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry-disc"))};

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
		auto trackId {track->getId()};

		const auto discNumber {track->getDiscNumber()};

		Wt::WContainerWidget* container {rootContainer};
		if (isReleaseMultiDisc && discNumber)
			container = getOrAddDiscContainer(*discNumber, track->getDiscSubtitle());

		Wt::WTemplate* entry {container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry"))};

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		const auto artists {track->getArtists({Database::TrackArtistLinkType::Artist})};
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
			tracksAction.emit(PlayQueueAction::Play, {trackId});
		});

		Wt::WText* moreBtn {entry->bindNew<Wt::WText>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML)};
		moreBtn->clicked().connect([=]()
		{
			displayTrackPopupMenu(*moreBtn, trackId, tracksAction);
		});

		entry->bindString("duration", trackDurationToString(track->getDuration()), Wt::TextFormat::Plain);

		LmsApp->getMediaPlayer().trackLoaded.connect(entry, [=] (Database::TrackId loadedTrackId)
		{
			entry->bindString("is-playing", loadedTrackId == trackId ? "Lms-entry-playing" : "");
		});

		if (auto trackIdLoaded {LmsApp->getMediaPlayer().getTrackLoaded()})
		{
			if (*trackIdLoaded == trackId)
				entry->bindString("is-playing", "Lms-entry-playing");
		}
		else
			entry->bindString("is-playing", "");
	}
}

void
Release::refreshReleaseArtists(const Database::Release::pointer& release)
{
	std::vector<Database::ObjectPtr<Database::Artist>> artists;

	artists = release->getReleaseArtists();
	if (artists.empty())
	{
		artists = release->getArtists(Database::TrackArtistLinkType::Artist);
		if (artists.size() > 1)
		{
			setCondition("if-has-various-release-artists", true);
			return;
		}
	}

	if (!artists.empty())
	{
		setCondition("if-has-release-artists", true);

		Wt::WContainerWidget* artistsContainer {bindNew<Wt::WContainerWidget>("artists")};
		for (const auto& artist : artists)
		{
			Wt::WTemplate* artistTemplate {artistsContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry-release-artist"))};
			artistTemplate->bindWidget("artist", LmsApplication::createArtistAnchor(artist));
		}
	}
}

void
Release::refreshCopyright(const Database::Release::pointer& release)
{
	std::optional<std::string> copyright {release->getCopyright()};
	std::optional<std::string> copyrightURL {release->getCopyrightURL()};

	if (!copyright && !copyrightURL)
		return;

	setCondition("if-has-copyright", true);

	std::string copyrightText {copyright ? *copyright : ""};
	if (copyrightText.empty() && copyrightURL)
		copyrightText = *copyrightURL;

	if (copyrightURL)
	{
		Wt::WLink link {*copyrightURL};
		link.setTarget(Wt::LinkTarget::NewWindow);

		Wt::WAnchor* anchor {bindNew<Wt::WAnchor>("copyright", link)};
		anchor->setTextFormat(Wt::TextFormat::Plain);
		anchor->setText(Wt::WString::fromUTF8(copyrightText));
	}
	else
		bindString("copyright", Wt::WString::fromUTF8(*copyright), Wt::TextFormat::Plain);
}

void
Release::refreshLinks(const Database::Release::pointer& release)
{
	const auto mbid {release->getMBID()};
	if (mbid)
	{
		setCondition("if-has-mbid", true);

		Wt::WLink link {"https://musicbrainz.org/release/" + std::string {mbid->getAsString()}};
		link.setTarget(Wt::LinkTarget::NewWindow);

		bindNew<Wt::WAnchor>("mbid-link", link, Wt::WString::tr("Lms.Explore.musicbrainz-release"));
	}
}

void
Release::refreshSimilarReleases(const std::vector<Database::ReleaseId>& similarReleasesId)
{
	if (similarReleasesId.empty())
		return;

	setCondition("if-has-similar-releases", true);
	auto* similarReleasesContainer {bindNew<Wt::WContainerWidget>("similar-releases")};

	for (const Database::ReleaseId id : similarReleasesId)
	{
		const Database::Release::pointer similarRelease{Database::Release::getById(LmsApp->getDbSession(), id)};
		if (!similarRelease)
			continue;

		similarReleasesContainer->addWidget(ReleaseListHelpers::createEntry(similarRelease));
	}
}

} // namespace UserInterface

