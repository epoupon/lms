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

#include <Wt/WMenu.h>
#include <Wt/WPopupMenu.h>
#include <Wt/WText.h>

#include "database/Session.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "ReleaseListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"

using namespace Database;

namespace UserInterface {

Releases::Releases(Filters& filters)
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.Releases.template")}
, _releaseCollector {filters, _defaultMode, _maxCount}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	{
		auto* menu {bindNew<Wt::WMenu>("mode")};

		auto addItem = [this](Wt::WMenu& menu,const Wt::WString& str, ReleaseCollector::Mode mode)
		{
			auto* item {menu.addItem(str)};
			item->clicked().connect([this, mode] { refreshView(mode); });

			if (mode == _defaultMode)
				item->renderSelected(true);
		};

		addItem(*menu, Wt::WString::tr("Lms.Explore.random"), ReleaseCollector::Mode::Random);
		addItem(*menu, Wt::WString::tr("Lms.Explore.starred"), ReleaseCollector::Mode::Starred);
		addItem(*menu, Wt::WString::tr("Lms.Explore.recently-played"), ReleaseCollector::Mode::RecentlyPlayed);
		addItem(*menu, Wt::WString::tr("Lms.Explore.most-played"), ReleaseCollector::Mode::MostPlayed);
		addItem(*menu, Wt::WString::tr("Lms.Explore.recently-added"), ReleaseCollector::Mode::RecentlyAdded);
		addItem(*menu, Wt::WString::tr("Lms.Explore.all"), ReleaseCollector::Mode::All);
	}

	Wt::WText* playBtn {bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML)};
	playBtn->clicked().connect([this]
	{
		releasesAction.emit(PlayQueueAction::Play, getAllReleases());
	});
	Wt::WText* moreBtn {bindNew<Wt::WText>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML)};
	moreBtn->clicked().connect([=]
	{
		Wt::WPopupMenu* popup {LmsApp->createPopupMenu()};

		popup->addItem(Wt::WString::tr("Lms.Explore.play-shuffled"))
			->triggered().connect([this]
			{
				releasesAction.emit(PlayQueueAction::PlayShuffled, getAllReleases());
			});
		popup->addItem(Wt::WString::tr("Lms.Explore.play-last"))
			->triggered().connect([this]
			{
				releasesAction.emit(PlayQueueAction::PlayLast, getAllReleases());
			});

		popup->popup(moreBtn);
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
	bool moreResults {};

	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const auto releases {_releaseCollector.get(Range {static_cast<std::size_t>(_container->getCount()), _batchSize}, moreResults)};
		for (const auto& release : releases)
			_container->add(ReleaseListHelpers::createEntry(release));
	}

	_container->setHasMore(moreResults);
}

std::vector<Database::ReleaseId>
Releases::getAllReleases()
{
	return _releaseCollector.getAll();
}

} // namespace UserInterface

