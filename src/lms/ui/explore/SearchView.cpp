/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "SearchView.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"

#include "resource/ImageResource.hpp"
#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "ReleaseListHelpers.hpp"
#include "LmsApplication.hpp"

static constexpr std::size_t maxEntries {5};

namespace UserInterface
{

	static
	std::unique_ptr<Wt::WTemplate>
	createTrack(const Database::Track::pointer& track)
	{
		auto entry {std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Search.template.track-entry"))};

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

		auto artists {track->getArtists()};
		auto release {track->getRelease()};
		const auto trackId {track.id()};

		if (!artists.empty() || release)
			entry->setCondition("if-has-artists-or-release", true);

		if (!artists.empty())
		{
			entry->setCondition("if-has-artists", true);

			Wt::WContainerWidget* artistContainer {entry->bindNew<Wt::WContainerWidget>("artists")};
			for (const auto& artist : artists)
			{
				Wt::WTemplate* a {artistContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Tracks.template.entry-artist"))};
				a->bindWidget("artist", LmsApplication::createArtistAnchor(artist));
			}
		}

		if (track->getRelease())
		{
			entry->setCondition("if-has-release", true);
			entry->bindWidget("release", LmsApplication::createReleaseAnchor(track->getRelease()));
			{
				Wt::WAnchor* anchor = entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));
				auto cover = std::make_unique<Wt::WImage>();
				cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 96));
				cover->setStyleClass("Lms-cover");
				anchor->setImage(std::move(cover));
			}
		}
		else
		{
			auto cover = entry->bindNew<Wt::WImage>("cover");
			cover->setImageLink(LmsApp->getImageResource()->getTrackUrl(trackId, 96));
			cover->setStyleClass("Lms-cover");
		}

		return entry;
	}

	SearchView::SearchView(Filters* filters)
	: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Search.template")}
	, _filters {filters}
	{
		addFunction("tr", &Wt::WTemplate::Functions::tr);

		_filters->updated().connect([=]
		{
			refreshView();
		});
	}

	void
	SearchView::refreshView(const std::string& searchText)
	{
		_keywords = StringUtils::splitString(searchText, " ");
		refreshView();
	}

	void
	SearchView::refreshView()
	{
		clear();

		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		searchArtists();
		searchReleases();
		searchTracks();
	}

	void
	SearchView::searchArtists()
	{
		bool more;
		const auto artists {Database::Artist::getByFilter(LmsApp->getDbSession(),
								_filters->getClusterIds(),
								_keywords,
								std::nullopt,
								Database::Artist::SortMethod::BySortName,
								Database::Range {0, maxEntries}, more)};

		if (!artists.empty())
		{
			setCondition("if-artists", true);

			auto* container {bindNew<Wt::WContainerWidget>("artists")};

			for (const Database::Artist::pointer& artist : artists)
				container->addWidget(ArtistListHelpers::createEntrySmall(artist));
		}
	}

	void
	SearchView::searchReleases()
	{
		bool more;
		const auto releases {Database::Release::getByFilter(LmsApp->getDbSession(),
								_filters->getClusterIds(),
								_keywords,
								Database::Range {0, maxEntries}, more)};

		if (!releases.empty())
		{
			setCondition("if-releases", true);

			auto* container {bindNew<Wt::WContainerWidget>("releases")};

			for (const Database::Release::pointer& release : releases)
				container->addWidget(ReleaseListHelpers::createEntrySmall(release));
		}
	}

	void
	SearchView::searchTracks()
	{
		bool more;
		const auto tracks {Database::Track::getByFilter(LmsApp->getDbSession(),
								_filters->getClusterIds(),
								_keywords,
								Database::Range {0, maxEntries}, more)};

		if (!tracks.empty())
		{
			setCondition("if-tracks", true);

			auto* container {bindNew<Wt::WContainerWidget>("tracks")};

			for (const Database::Track::pointer& track : tracks)
				container->addWidget(createTrack(track));
		}
	}

} // namespace UserInterface

