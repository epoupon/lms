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

#include "TrackListsView.hpp"

#include <Wt/WPushButton.h>

#include "services/database/Session.hpp"
#include "services/database/TrackList.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "common/Template.hpp"
#include "resource/DownloadResource.hpp"
#include "ReleaseListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "ModalManager.hpp"
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
	TrackLists::refreshView()
	{
		_container->clear();
		addSome();
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

		const auto trackListIds {TrackList::find(session, params)};
		for (const TrackListId trackListId : trackListIds.results)
		{
			if (const TrackList::pointer trackList {TrackList::find(LmsApp->getDbSession(), trackListId)})
				addTracklist(trackList);
		}

		_container->setHasMore(trackListIds.moreResults && _container->getCount() <= _maxCount);
	}

	void
	TrackLists::addTracklist(const ObjectPtr<TrackList>& trackList)
	{
		const TrackListId trackListId {trackList->getId()};
		Template* entry {_container->addNew<Template>(Wt::WString::tr("Lms.Explore.TrackLists.template.entry"))};
		entry->addFunction("id", &Wt::WTemplate::Functions::id);

		entry->bindString("name", std::string {trackList->getName()}, Wt::TextFormat::Plain);
		entry->bindString("duration", Utils::durationToString(trackList->getDuration()));

		entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML)
			->clicked().connect([=]
			{
				trackListAction.emit(PlayQueueAction::Play, trackListId);
			});

		entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML);

		entry->bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"))
			->clicked().connect([=]
			{
				trackListAction.emit(PlayQueueAction::PlayShuffled, trackListId);
			});

		entry->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
			->clicked().connect([=]
			{
				trackListAction.emit(PlayQueueAction::PlayLast, trackListId);
			});

		entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
			->setLink(Wt::WLink {std::make_unique<DownloadTrackListResource>(trackListId)});

		entry->bindNew<Wt::WPushButton>("delete", Wt::WString::tr("Lms.delete"))
			->clicked().connect([=]
			{
				auto modal {std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.TrackLists.template.delete-tracklist"))};
				modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
				Wt::WWidget* modalPtr {modal.get()};

				auto* delBtn {modal->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.delete"))};
				delBtn->clicked().connect([=]
				{
					{
						auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

						TrackList::pointer trackList {TrackList::find(LmsApp->getDbSession(), trackListId)};
						if (trackList)
							trackList.remove();
					}

					_container->remove(*entry);

					LmsApp->getModalManager().dispose(modalPtr);
				});

				auto* cancelBtn {modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel"))};
				cancelBtn->clicked().connect([=]
				{
					LmsApp->getModalManager().dispose(modalPtr);
				});

				LmsApp->getModalManager().show(std::move(modal));
			});
	}

} // namespace UserInterface

