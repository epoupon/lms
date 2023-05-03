/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "TrackListsView.hpp"

#include <Wt/WPushButton.h>

#include "services/database/Session.hpp"
#include "services/database/TrackList.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "common/Template.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "Utils.hpp"

using namespace Database;

namespace UserInterface
{
	TrackLists::TrackLists(Filters& filters)
	: Template {Wt::WString::tr("Lms.Explore.TrackLists.template")}
	, _filters {filters}
	{
		addFunction("tr", &Wt::WTemplate::Functions::tr);
		addFunction("id", &Wt::WTemplate::Functions::id);

		auto bindMenuItem {[this](const std::string& var, const Wt::WString& title, Mode mode)
		{
			auto *menuItem {bindNew<Wt::WPushButton>(var, title)};
			menuItem->clicked().connect([=]
			{
				_mode = mode;
				refreshView();
				_currentActiveItem->removeStyleClass("active");
				menuItem->addStyleClass("active");
				_currentActiveItem = menuItem;
			});

			if (mode == _mode)
			{
				_currentActiveItem = menuItem;
				_currentActiveItem->addStyleClass("active");
			}
		}};

		bindMenuItem("recently-modified", Wt::WString::tr("Lms.Explore.recently-modified"), Mode::RecentlyModified);
		bindMenuItem("all", Wt::WString::tr("Lms.Explore.all"), Mode::All);

		_container = bindNew<InfiniteScrollingContainer>("tracklists", Wt::WString::tr("Lms.Explore.TrackLists.template.container"));
		_container->onRequestElements.connect([this]
		{
			addSome();
		});

		_filters.updated().connect([this]
		{
			refreshView();
		});

		refreshView();
	}

	void
	TrackLists::onTrackListDeleted(Database::TrackListId trackListId)
	{
		auto itTrackList {_trackListWidgets.find(trackListId)};
		if (itTrackList == std::end(_trackListWidgets))
			return;

		_container->remove(*itTrackList->second);
		_trackListWidgets.erase(itTrackList);
	}

	void
	TrackLists::refreshView()
	{
		_container->reset();
		_trackListWidgets.clear();
	}

	void
	TrackLists::addSome()
	{
		const Range range {static_cast<std::size_t>(_container->getCount()), _batchSize};

		Session& session {LmsApp->getDbSession()};
		auto transaction {session.createSharedTransaction()};

		TrackList::FindParameters params;
		params.setClusters(_filters.getClusterIds());
		params.setUser(LmsApp->getUserId());
		params.setType(TrackListType::Playlist);
		params.setRange(range);
		switch (_mode)
		{
			case Mode::All:
				params.setSortMethod(TrackListSortMethod::Name);
				break;
			case Mode::RecentlyModified:
				params.setSortMethod(TrackListSortMethod::LastModifiedDesc);
				break;
		}

		const auto trackListIds {TrackList::find(session, params)};
		for (const TrackListId trackListId : trackListIds.results)
		{
			if (const TrackList::pointer trackList {TrackList::find(LmsApp->getDbSession(), trackListId)})
				addTracklist(trackList);
		}
	}

	void
	TrackLists::addTracklist(const ObjectPtr<TrackList>& trackList)
	{
		const TrackListId trackListId {trackList->getId()};

		WTemplate* entry {_container->addNew<Template>(Wt::WString::tr("Lms.Explore.TrackLists.template.entry"))};
		entry->bindWidget("name", Utils::createTrackListAnchor(trackList));

		assert(_trackListWidgets.find(trackListId) == std::cend(_trackListWidgets));
		_trackListWidgets.emplace(trackListId, entry);
	}

} // namespace UserInterface

