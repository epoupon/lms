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

#include "ArtistView.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "recommendation/IEngine.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "resource/ImageResource.hpp"
#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "ReleaseListHelpers.hpp"

using namespace Database;

namespace UserInterface {

Artist::Artist(Filters* filters)
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Artist.template")}
, _filters(filters)
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	LmsApp->internalPathChanged().connect([=]
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
Artist::refreshView()
{
	if (!wApp->internalPathMatches("/artist/"))
		return;

	clear();

	const auto artistId {StringUtils::readAs<Database::IdType>(wApp->internalPathNextPart("/artist/"))};
	if (!artistId)
		return;

	const std::vector<Database::IdType> similarArtistIds {ServiceProvider<Recommendation::IEngine>::get()->getSimilarArtists(LmsApp->getDbSession(), *artistId, 5)};

    auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Artist::pointer artist {Database::Artist::getById(LmsApp->getDbSession(), *artistId)};
	if (!artist)
		throw ArtistNotFoundException {*artistId};

	refreshLinks(artist);
	refreshSimilarArtists(similarArtistIds);

	Wt::WContainerWidget* clusterContainers = bindNew<Wt::WContainerWidget>("clusters");

	{
		auto clusterTypes = ScanSettings::get(LmsApp->getDbSession())->getClusterTypes();
		auto clusterGroups = artist->getClusterGroups(clusterTypes, 3);

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

	bindString("name", Wt::WString::fromUTF8(artist->getName()), Wt::TextFormat::Plain);
	{
		Wt::WText* playBtn = bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML);

		playBtn->clicked().connect([=]
		{
			artistsAction.emit(PlayQueueAction::Play, {*artistId});
		});
	}

	{
		Wt::WText* addBtn = bindNew<Wt::WText>("add-btn", Wt::WString::tr("Lms.Explore.template.add-btn"), Wt::TextFormat::XHTML);

		addBtn->clicked().connect([=]
		{
			artistsAction.emit(PlayQueueAction::AddLast, {*artistId});
		});
	}

	Wt::WContainerWidget* releasesContainer = bindNew<Wt::WContainerWidget>("releases");

	auto releases = artist->getReleases(_filters->getClusterIds());
	for (const auto& release : releases)
	{
		releasesContainer->addWidget(ReleaseListHelpers::createEntryForArtist(release, artist));
	}
}

void
Artist::refreshSimilarArtists(const std::vector<Database::IdType>& similarArtistsId)
{
	Wt::WContainerWidget* similarArtistsContainer {bindNew<Wt::WContainerWidget>("similar-artists")};

	for (Database::IdType artistId : similarArtistsId)
	{
		const Database::Artist::pointer similarArtist{Database::Artist::getById(LmsApp->getDbSession(), artistId)};
		if (!similarArtist)
			continue;

		similarArtistsContainer->addWidget(ArtistListHelpers::createEntrySmall(similarArtist));
	}
}

void
Artist::refreshLinks(const Database::Artist::pointer& artist)
{
	const auto mbid {artist->getMBID()};
	if (mbid)
	{
		setCondition("if-has-mbid", true);

		Wt::WLink link {"https://musicbrainz.org/artist/" + std::string {mbid->getAsString()}};
		link.setTarget(Wt::LinkTarget::NewWindow);

		bindNew<Wt::WAnchor>("mbid-link", link, Wt::WString::tr("Lms.Explore.musicbrainz-artist"));
	}
}

} // namespace UserInterface

