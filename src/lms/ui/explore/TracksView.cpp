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

#include "TracksView.hpp"

#include <Wt/WPushButton.h>

#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "utils/Logger.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "TrackListHelpers.hpp"

using namespace Database;

namespace UserInterface {

Tracks::Tracks(Filters& filters)
: Template {Wt::WString::tr("Lms.Explore.Tracks.template")},
_trackCollector {filters, _defaultMode, _maxCount}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);
	addFunction("id", &Wt::WTemplate::Functions::id);

	auto bindMenuItem {[this](const std::string& var, const Wt::WString& title, TrackCollector::Mode mode)
	{
		auto *menuItem {bindNew<Wt::WPushButton>(var, title)};
		menuItem->clicked().connect([=]
		{
			refreshView(mode);
			_currentActiveItem->removeStyleClass("active");
			menuItem->addStyleClass("active");
			_currentActiveItem = menuItem;
		});

		if (mode == _defaultMode)
		{
			_currentActiveItem = menuItem;
			_currentActiveItem->addStyleClass("active");
		}
	}};

	bindMenuItem("random", Wt::WString::tr("Lms.Explore.random"), TrackCollector::Mode::Random);
	bindMenuItem("starred", Wt::WString::tr("Lms.Explore.starred"), TrackCollector::Mode::Starred);
	bindMenuItem("recently-played", Wt::WString::tr("Lms.Explore.recently-played"), TrackCollector::Mode::RecentlyPlayed);
	bindMenuItem("most-played", Wt::WString::tr("Lms.Explore.most-played"), TrackCollector::Mode::MostPlayed);
	bindMenuItem("recently-added", Wt::WString::tr("Lms.Explore.recently-added"), TrackCollector::Mode::RecentlyAdded);
	bindMenuItem("all", Wt::WString::tr("Lms.Explore.all"), TrackCollector::Mode::All);

	bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
		->clicked().connect([=]
		{
			tracksAction.emit(PlayQueueAction::Play, getAllTracks());
		});

	bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
		->clicked().connect([=]
		{
			tracksAction.emit(PlayQueueAction::PlayShuffled, getAllTracks());
		});
	bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
		->clicked().connect([=]
		{
			tracksAction.emit(PlayQueueAction::PlayLast, getAllTracks());
		});

	_container = bindNew<InfiniteScrollingContainer>("tracks");
	_container->onRequestElements.connect([this]
	{
		addSome();
	});

	filters.updated().connect([this]
	{
		refreshView();
	});

	refreshView(_trackCollector.getMode());
}

void
Tracks::refreshView()
{
	_container->clear();
	_trackCollector.reset();
	addSome();
}

void
Tracks::refreshView(TrackCollector::Mode mode)
{
	_trackCollector.setMode(mode);
	refreshView();
}

void
Tracks::addSome()
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const auto trackIds {_trackCollector.get(Range {static_cast<std::size_t>(_container->getCount()), _batchSize})};

	for (const TrackId trackId : trackIds.results)
	{
		if (const Track::pointer track {Track::find(LmsApp->getDbSession(), trackId)})
			_container->add(TrackListHelpers::createEntry(track, tracksAction));
	}

	_container->setHasMore(trackIds.moreResults);
}

std::vector<Database::TrackId>
Tracks::getAllTracks()
{
	RangeResults<TrackId> trackIds {_trackCollector.get(Range {})};

	return std::move(trackIds.results);
}


} // namespace UserInterface

