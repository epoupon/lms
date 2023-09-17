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

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "ReleaseHelpers.hpp"
#include "TrackListHelpers.hpp"

using namespace Database;

namespace UserInterface
{
	SearchView::SearchView(Filters& filters, PlayQueueController& playQueueController)
	: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Search.template")}
	, _playQueueController {playQueueController}
	, _filters {filters}
	, _artistCollector {filters, ArtistCollector::Mode::Search, getMaxCount(Mode::Artist)}
	, _releaseCollector {filters, ReleaseCollector::Mode::Search, getMaxCount(Mode::Release)}
	, _trackCollector {filters, TrackCollector::Mode::Search, getMaxCount(Mode::Track)}
	{
		addFunction("tr", &Wt::WTemplate::Functions::tr);

		_artists = bindNew<InfiniteScrollingContainer>("artists", Wt::WString::tr("Lms.Explore.Artists.template.container"));
		_artists->onRequestElements.connect([this] { addSomeArtists(); });

		_artistLinkType = bindNew<Wt::WComboBox>("link-type");
		_artistLinkType->setModel(ArtistListHelpers::createArtistLinkTypesModel());
		_artistLinkType->changed().connect([this]
		{
			const std::optional<TrackArtistLinkType> linkType {static_cast<ArtistLinkTypesModel*>(_artistLinkType->model().get())->getValue(_artistLinkType->currentIndex())};
			refreshView(linkType);
		});

		_releases = bindNew<InfiniteScrollingContainer>("releases", Wt::WString::tr("Lms.Explore.Releases.template.container"));
		_releases->onRequestElements.connect([this] { addSomeReleases(); });

		_tracks = bindNew<InfiniteScrollingContainer>("tracks");
		_tracks->onRequestElements.connect([this] { addSomeTracks(); });

		filters.updated().connect([=]
		{
			refreshView();
		});

		LmsApp->getScannerEvents().scanComplete.connect(this, [this](const Scanner::ScanStats& stats)
		{
			if (stats.nbChanges())
				_artistLinkType->setModel(ArtistListHelpers::createArtistLinkTypesModel());
		});
	}

	std::size_t
	SearchView::getBatchSize(Mode mode) const
	{
		auto it {_batchSizes.find(mode)};
		assert(it != _batchSizes.cend());
		return it->second;
	}

	std::size_t
	SearchView::getMaxCount(Mode mode) const
	{
		auto it {_maxCounts.find(mode)};
		assert(it != _maxCounts.cend());
		return it->second;
	}

	void
	SearchView::refreshView(std::optional<TrackArtistLinkType> linkType)
	{
		_artistCollector.setArtistLinkType(linkType);
		refreshView();
	}

	void
	SearchView::refreshView(const Wt::WString& searchText)
	{
		_releaseCollector.setSearch(searchText.toUTF8());
		_artistCollector.setSearch(searchText.toUTF8());
		_trackCollector.setSearch(searchText.toUTF8());
		refreshView();
	}

	void
	SearchView::refreshView()
	{
		_artists->reset();
		_releases->reset();
		_tracks->reset();

		addSomeArtists();
		addSomeReleases();
		addSomeTracks();
	}

	void
	SearchView::addSomeArtists()
	{
		using namespace Database;

		const Range range {_artists->getCount(), getBatchSize(Mode::Artist)};
		const RangeResults<ArtistId> artistIds {_artistCollector.get(range)};
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			for (const ArtistId artistId : artistIds.results)
			{
				const Artist::pointer artist {Artist::find(LmsApp->getDbSession(), artistId)};
				_artists->add(ArtistListHelpers::createEntry(artist));
			}
		}
	}

	void
	SearchView::addSomeReleases()
	{
		using namespace Database;

		const Range range {_releases->getCount(), getBatchSize(Mode::Release)};
		const RangeResults<ReleaseId> releaseIds {_releaseCollector.get(range)};
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			for (const ReleaseId releaseId : releaseIds.results)
			{
				const Release::pointer release {Release::find(LmsApp->getDbSession(), releaseId)};
				_releases->add(ReleaseListHelpers::createEntry(release));
			}
		}
	}

	void
	SearchView::addSomeTracks()
	{
		using namespace Database;

		const Range range {_tracks->getCount(), getBatchSize(Mode::Track)};
		const RangeResults<TrackId> trackIds {_trackCollector.get(range)};

		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			for (const TrackId trackId : trackIds.results)
			{
				const Track::pointer track {Track::find(LmsApp->getDbSession(), trackId)};
				_tracks->add(TrackListHelpers::createEntry(track, _playQueueController, _filters));
			}
		}
	}
} // namespace UserInterface

