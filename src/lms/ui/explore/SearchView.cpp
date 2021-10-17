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

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "common/LoadingIndicator.hpp"
#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "ReleaseListHelpers.hpp"
#include "TrackListHelpers.hpp"

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
		stack->setAttributeValue("style", "overflow-x:visible;overflow-y:visible;");
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
		addItem(Wt::WString::tr("Lms.Explore.tracks"), Mode::Track, Wt::WString::tr("Lms.Explore.Tracks.template.container"), [this]{ addSomeTracks(); });

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
		bool moreResults {};

		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::Range range {results.getCount(), getBatchSize(Mode::Artist)};
			for (const auto& artist : _artistCollector.get(range, moreResults))
				results.add(ArtistListHelpers::createEntry(artist));
		}

		results.setHasMore(moreResults);

		getItemMenu(Mode::Artist).setDisabled(results.getCount() == 0);
	}

	void
	SearchView::addSomeReleases()
	{
		InfiniteScrollingContainer& results {getResultContainer(Mode::Release)};
		bool moreResults {};

		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::Range range {results.getCount(), getBatchSize(Mode::Release)};
			for (const auto& release : _releaseCollector.get(range, moreResults))
				results.add(ReleaseListHelpers::createEntry(release));
		}

		results.setHasMore(moreResults);

		getItemMenu(Mode::Release).setDisabled(results.getCount() == 0);
	}

	void
	SearchView::addSomeTracks()
	{
		InfiniteScrollingContainer& results {getResultContainer(Mode::Track)};
		bool moreResults {};

		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::Range range {results.getCount(), getBatchSize(Mode::Track)};
			for (const auto& track : _trackCollector.get(range, moreResults))
				results.add(TrackListHelpers::createEntry(track, tracksAction));
		}

		results.setHasMore(moreResults);

		getItemMenu(Mode::Track).setDisabled(results.getCount() == 0);
	}



} // namespace UserInterface

