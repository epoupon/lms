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

#include <functional>

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WStackedWidget.h>

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "common/LoadingIndicator.hpp"
#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "ReleaseListHelpers.hpp"
#include "TrackListHelpers.hpp"

using namespace Database;

namespace UserInterface
{
	SearchView::SearchView(Filters* filters)
	: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Search.template")}
	, _filters {filters}
	, _releaseCollector {*filters, ReleaseCollector::Mode::Search, getMaxCount(Mode::Release)}
	, _artistCollector {*filters, ArtistCollector::Mode::Search, getMaxCount(Mode::Artist)}
	, _trackCollector {*filters, TrackCollector::Mode::Search, getMaxCount(Mode::Track)}
	{
		addFunction("tr", &Wt::WTemplate::Functions::tr);

		Wt::WStackedWidget* stack {bindNew<Wt::WStackedWidget>("stack")};
		_menu = bindNew<Wt::WMenu>("mode", stack);

		auto addItem = [=](const Wt::WString& str, [[maybe_unused]] Mode mode, const Wt::WString& templateStr, std::function<void()> onRequestElementsFunc)
		{
			assert(modeToIndex(mode) == _results.size());

			auto results {std::make_unique<InfiniteScrollingContainer>(templateStr)};
			results->onRequestElements.connect(std::move(onRequestElementsFunc));

			_results.push_back(results.get());
			_menu->addItem(str, std::move(results));
		};

		// same order as Mode!
		addItem(Wt::WString::tr("Lms.Explore.releases"), Mode::Release, Wt::WString::tr("Lms.Explore.Releases.template.container"), [this]{ addSomeReleases(); });
		addItem(Wt::WString::tr("Lms.Explore.artists"), Mode::Artist, Wt::WString::tr("Lms.infinite-scrolling-container"), [this]{ addSomeArtists(); });
		addItem(Wt::WString::tr("Lms.Explore.tracks"), Mode::Track, Wt::WString::tr("Lms.infinite-scrolling-container"), [this]{ addSomeTracks(); });

		_filters->updated().connect([=]
		{
			refreshView();
		});
	}

	std::size_t
	SearchView::modeToIndex(Mode mode) const
	{
		return static_cast<std::size_t>(mode);
	}

	Wt::WMenuItem&
	SearchView::getItemMenu(Mode mode) const
	{
		return *_menu->itemAt(modeToIndex(mode));
	}

	InfiniteScrollingContainer&
	SearchView::getResultContainer(Mode mode) const
	{
		return *_results[modeToIndex(mode)];
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
		for (InfiniteScrollingContainer* results : _results)
			results->clear();

		addSomeReleases();
		addSomeArtists();
		addSomeTracks();
	}

	void
	SearchView::addSomeArtists()
	{
		InfiniteScrollingContainer& results {getResultContainer(Mode::Artist)};

		{
			using namespace Database;

			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Range range {results.getCount(), getBatchSize(Mode::Artist)};
			const RangeResults<ArtistId> artistIds {_artistCollector.get(range)};
			for (const ArtistId artistId : artistIds.results)
			{
				const Artist::pointer artist {Artist::find(LmsApp->getDbSession(), artistId)};
				results.add(ArtistListHelpers::createEntry(artist));
			}

			results.setHasMore(artistIds.moreResults);
		}

		getItemMenu(Mode::Artist).setDisabled(results.getCount() == 0);
	}

	void
	SearchView::addSomeReleases()
	{
		InfiniteScrollingContainer& results {getResultContainer(Mode::Release)};

		{
			using namespace Database;

			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Range range {results.getCount(), getBatchSize(Mode::Release)};
			const RangeResults<ReleaseId> releaseIds {_releaseCollector.get(range)};

			for (const ReleaseId releaseId : releaseIds.results)
			{
				const Release::pointer release {Release::find(LmsApp->getDbSession(), releaseId)};
				results.add(ReleaseListHelpers::createEntry(release));
			}

			results.setHasMore(releaseIds.moreResults);
		}

		getItemMenu(Mode::Release).setDisabled(results.getCount() == 0);
	}

	void
	SearchView::addSomeTracks()
	{
		InfiniteScrollingContainer& results {getResultContainer(Mode::Track)};
		{
			using namespace Database;

			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Range range {results.getCount(), getBatchSize(Mode::Track)};
			const RangeResults<TrackId> trackIds {_trackCollector.get(range)};

			for (const TrackId trackId : trackIds.results)
			{
				const Track::pointer track {Track::find(LmsApp->getDbSession(), trackId)};
				results.add(TrackListHelpers::createEntry(track, tracksAction));
			}

			results.setHasMore(trackIds.moreResults);
		}

		getItemMenu(Mode::Track).setDisabled(results.getCount() == 0);
	}
} // namespace UserInterface

