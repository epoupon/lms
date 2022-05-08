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

#include "ReleasesView.hpp"

#include <Wt/WPushButton.h>

#include "services/database/Release.hpp"
#include "services/database/Session.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "common/Template.hpp"
#include "ReleaseListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

using namespace Database;

namespace UserInterface {

Releases::Releases(Filters& filters)
: Template {Wt::WString::tr("Lms.Explore.Releases.template")}
, _releaseCollector {filters, _defaultMode, _maxCount}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);
	addFunction("id", &Wt::WTemplate::Functions::id);

	auto bindMenuItem {[this](const std::string& var, const Wt::WString& title, ReleaseCollector::Mode mode)
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

	bindMenuItem("random", Wt::WString::tr("Lms.Explore.random"), ReleaseCollector::Mode::Random);
	bindMenuItem("starred", Wt::WString::tr("Lms.Explore.starred"), ReleaseCollector::Mode::Starred);
	bindMenuItem("recently-played", Wt::WString::tr("Lms.Explore.recently-played"), ReleaseCollector::Mode::RecentlyPlayed);
	bindMenuItem("most-played", Wt::WString::tr("Lms.Explore.most-played"), ReleaseCollector::Mode::MostPlayed);
	bindMenuItem("recently-added", Wt::WString::tr("Lms.Explore.recently-added"), ReleaseCollector::Mode::RecentlyAdded);
	bindMenuItem("all", Wt::WString::tr("Lms.Explore.all"), ReleaseCollector::Mode::All);

	Wt::WPushButton* playBtn {bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)};
	playBtn->clicked().connect([this]
	{
		releasesAction.emit(PlayQueueAction::Play, getAllReleases());
	});

	bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML);
	bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
		->clicked().connect([=]
		{
			releasesAction.emit(PlayQueueAction::PlayShuffled, getAllReleases());
		});
	bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
		->clicked().connect([=]
		{
			releasesAction.emit(PlayQueueAction::PlayLast, getAllReleases());
		});

	_container = bindNew<InfiniteScrollingContainer>("releases", Wt::WString::tr("Lms.Explore.Releases.template.container"));
	_container->onRequestElements.connect([this]
	{
		addSome();
	});

	filters.updated().connect([this]
	{
		refreshView();
	});

	refreshView(_releaseCollector.getMode());
}

void
Releases::refreshView()
{
	_container->clear();
	_releaseCollector.reset();
	addSome();
}

void
Releases::refreshView(ReleaseCollector::Mode mode)
{
	_releaseCollector.setMode(mode);
	refreshView();
}

void
Releases::addSome()
{
	const auto releaseIds {_releaseCollector.get(Range {static_cast<std::size_t>(_container->getCount()), _batchSize})};

	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		for (const ReleaseId releaseId : releaseIds.results)
		{
			if (const Release::pointer release {Release::find(LmsApp->getDbSession(), releaseId)})
				_container->add(ReleaseListHelpers::createEntry(release));
		}
	}

	_container->setHasMore(releaseIds.moreResults);
}

std::vector<ReleaseId>
Releases::getAllReleases()
{
	RangeResults<ReleaseId> releaseIds {_releaseCollector.get(Range {})};

	return std::move(releaseIds.results);
}

} // namespace UserInterface

