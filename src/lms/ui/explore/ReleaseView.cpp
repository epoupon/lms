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
#include <Wt/WImage.h>
#include <Wt/WPushButton.h>

#include "services/database/Cluster.hpp"
#include "services/database/Release.hpp"
#include "services/database/ScanSettings.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Logger.hpp"

#include "common/Template.hpp"
#include "resource/DownloadResource.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "MediaPlayer.hpp"
#include "ReleaseListHelpers.hpp"
#include "Utils.hpp"

using namespace Database;

namespace UserInterface {

Release::Release(Filters* filters)
: Template {Wt::WString::tr("Lms.Explore.Release.template")}
, _filters {filters}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);
	addFunction("id", &Wt::WTemplate::Functions::id);

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
			if (const Database::Release::pointer release {Database::Release::find(LmsApp->getDbSession(), *mbid)})
				return release->getId();
		}

		return std::nullopt;
	}

	return StringUtils::readAs<ReleaseId::ValueType>(wApp->internalPathNextPart("/release/"));
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

	auto similarReleasesIds {Service<Recommendation::IRecommendationService>::get()->getSimilarReleases(*releaseId, 6)};

    auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Release::pointer release {Database::Release::find(LmsApp->getDbSession(), *releaseId)};
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

	bindString("duration", Utils::durationToString(release->getDuration()), Wt::TextFormat::Plain);

	refreshReleaseArtists(release);

	bindWidget<Wt::WImage>("cover", Utils::createCover(release->getId(), CoverResource::Size::Large));

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

	bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML);
	bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)
		->clicked().connect([=]
		{
			releasesAction.emit(PlayQueueAction::Play, {*releaseId});
		});

	bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
		->clicked().connect([=]
		{
			releasesAction.emit(PlayQueueAction::PlayShuffled, {*releaseId});
		});

	bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
		->clicked().connect([=]
		{
			releasesAction.emit(PlayQueueAction::PlayLast, {*releaseId});
		});

	bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
		->setLink(Wt::WLink {std::make_unique<DownloadReleaseResource>(*releaseId)});

	{
		auto isStarred {[=] { return Service<Scrobbling::IScrobblingService>::get()->isStarred(LmsApp->getUserId(), *releaseId); }};

		Wt::WPushButton* starBtn {bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star"))};
		starBtn->clicked().connect([=]
		{
			if (isStarred())
			{
				Service<Scrobbling::IScrobblingService>::get()->unstar(LmsApp->getUserId(), *releaseId);
				starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
			}
			else
			{
				Service<Scrobbling::IScrobblingService>::get()->star(LmsApp->getUserId(), *releaseId);
				starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
			}
		});
	}

	Wt::WContainerWidget* rootContainer {bindNew<Wt::WContainerWidget>("container")};

	const bool variousArtists {release->hasVariousArtists()};
	const auto totalDisc {release->getTotalDisc()};
	const bool isReleaseMultiDisc {totalDisc && *totalDisc > 1};

	// Expect to be called in asc order
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

	Wt::WContainerWidget* noDiscTracksContainer{};
	auto getOrAddNoDiscContainer =  [&]
	{
		if (noDiscTracksContainer)
			return noDiscTracksContainer;

		Wt::WTemplate* disc {rootContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry-nodisc"))};
		noDiscTracksContainer = disc->bindNew<Wt::WContainerWidget>("tracks");

		return noDiscTracksContainer;
	};

	const auto clusterIds {_filters->getClusterIds()};
	const auto tracks {release->getTracks(clusterIds)};

	for (const auto& track : tracks)
	{
		auto trackId {track->getId()};

		const auto discNumber {track->getDiscNumber()};

		Wt::WContainerWidget* container;
		if (isReleaseMultiDisc && discNumber)
			container = getOrAddDiscContainer(*discNumber, track->getDiscSubtitle());
		else
			container = getOrAddNoDiscContainer();

		Template* entry {container->addNew<Template>(Wt::WString::tr("Lms.Explore.Release.template.entry"))};

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		const auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		if (variousArtists && !artists.empty())
		{
			entry->setCondition("if-has-artists", true);

			Wt::WContainerWidget* artistsContainer {entry->bindNew<Wt::WContainerWidget>("artists")};
			bool firstArtist {true};
			for (const auto& artist : artists)
			{
				if (!firstArtist)
					artistsContainer->addNew<Wt::WText>(" · ");
				auto anchor {LmsApplication::createArtistAnchor(artist)};
				anchor->addStyleClass("link-success text-decoration-none"); // hack
				artistsContainer->addWidget(std::move(anchor));
				firstArtist = false;
			}
		}

		auto trackNumber {track->getTrackNumber()};
		if (trackNumber)
		{
			entry->setCondition("if-has-track-number", true);
			entry->bindInt("track-number", *trackNumber);
		}

		Wt::WPushButton* playBtn {entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)};
		playBtn->clicked().connect([=]
		{
			tracksAction.emit(PlayQueueAction::Play, {trackId});
		});

		{
			entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML);
			entry->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
				->clicked().connect([=]
				{
					tracksAction.emit(PlayQueueAction::PlayLast, {trackId});
				});

			auto isStarred {[=] { return Service<Scrobbling::IScrobblingService>::get()->isStarred(LmsApp->getUserId(), trackId); }};

			Wt::WPushButton* starBtn {entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star"))};
			starBtn->clicked().connect([=]
			{
				auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

				if (isStarred())
				{
					Service<Scrobbling::IScrobblingService>::get()->unstar(LmsApp->getUserId(), trackId);
					starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
				}
				else
				{
					Service<Scrobbling::IScrobblingService>::get()->star(LmsApp->getUserId(), trackId);
					starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
				}
			});

			entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
				->setLink(Wt::WLink {std::make_unique<DownloadTrackResource>(trackId)});
		}

		entry->bindString("duration", Utils::durationToString(track->getDuration()), Wt::TextFormat::Plain);

		LmsApp->getMediaPlayer().trackLoaded.connect(entry, [=] (TrackId loadedTrackId)
		{
			entry->toggleStyleClass("Lms-entry-playing", loadedTrackId == trackId);
		});

		if (auto trackIdLoaded {LmsApp->getMediaPlayer().getTrackLoaded()})
		{
			entry->toggleStyleClass("Lms-entry-playing", *trackIdLoaded == trackId);
		}
		else
			entry->removeStyleClass("Lms-entry-playing");
	}
}

void
Release::refreshReleaseArtists(const Database::Release::pointer& release)
{
	std::vector<ObjectPtr<Artist>> artists;

	artists = release->getReleaseArtists();
	if (artists.empty())
	{
		artists = release->getArtists(TrackArtistLinkType::Artist);
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
		bindString("mbid-link", std::string {"https://musicbrainz.org/release/"} + std::string {mbid->getAsString()});
	}
}

void
Release::refreshSimilarReleases(const std::vector<ReleaseId>& similarReleasesId)
{
	if (similarReleasesId.empty())
		return;

	setCondition("if-has-similar-releases", true);
	auto* similarReleasesContainer {bindNew<Wt::WContainerWidget>("similar-releases")};

	for (const ReleaseId id : similarReleasesId)
	{
		const Database::Release::pointer similarRelease {Database::Release::find(LmsApp->getDbSession(), id)};
		if (!similarRelease)
			continue;

		similarReleasesContainer->addWidget(ReleaseListHelpers::createEntry(similarRelease));
	}
}

} // namespace UserInterface

