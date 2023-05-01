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

#include "TrackListView.hpp"

#include <Wt/WPushButton.h>

#include "services/database/Cluster.hpp"
#include "services/database/ScanSettings.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackList.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "explore/Filters.hpp"
#include "explore/PlayQueueController.hpp"
#include "explore/TrackListHelpers.hpp"
#include "resource/DownloadResource.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "ModalManager.hpp"
#include "Utils.hpp"

using namespace Database;

namespace
{
	static
	std::optional<TrackListId>
	extractTrackListIdFromInternalPath()
	{
		return StringUtils::readAs<TrackListId::ValueType>(wApp->internalPathNextPart("/tracklist/"));
	}
}

namespace UserInterface
{
	TrackList::TrackList(Filters& filters, PlayQueueController& playQueueController)
	: Template {Wt::WString::tr("Lms.Explore.TrackList.template")}
	, _filters {filters}
	, _playQueueController {playQueueController}
	{
		addFunction("tr", &Wt::WTemplate::Functions::tr);
		addFunction("id", &Wt::WTemplate::Functions::id);

		wApp->internalPathChanged().connect(this, [this]
		{
			refreshView();
		});

		_filters.updated().connect([this]
		{
			refreshView();
		});

		refreshView();
	}

	void
	TrackList::refreshView()
	{
		if (!wApp->internalPathMatches("/tracklist/"))
			return;

		const std::optional<TrackListId> trackListId {extractTrackListIdFromInternalPath()};
		if (!trackListId)
			throw TrackListNotFoundException {};

		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const Database::TrackList::pointer trackList {Database::TrackList::find(LmsApp->getDbSession(), *trackListId)};
		if (!trackList)
			throw TrackListNotFoundException {};

		LmsApp->setTitle(std::string {trackList->getName()});
		_trackListId = *trackListId;

		clear();

		bindString("name", std::string {trackList->getName()}, Wt::TextFormat::Plain);
		bindString("duration", Utils::durationToString(trackList->getDuration()));
		const auto trackCount {trackList->getCount()};
		bindString("track-count", Wt::WString::trn("Lms.track-count", trackCount).arg(trackCount));

		Wt::WContainerWidget* clusterContainers {bindNew<Wt::WContainerWidget>("clusters")};
		{
			const auto clusterTypes {ScanSettings::get(LmsApp->getDbSession())->getClusterTypes()};
			const auto clusterGroups {trackList->getClusterGroups(clusterTypes, 3)};

			for (const auto& clusters : clusterGroups)
			{
				for (const Database::Cluster::pointer& cluster : clusters)
				{
					const ClusterId clusterId {cluster->getId()};
					Wt::WInteractWidget* entry {clusterContainers->addWidget(Utils::createCluster(clusterId))};
					entry->clicked().connect([=]
					{
						_filters.add(clusterId);
					});
				}
			}
		}

		bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
			->clicked().connect([=]
			{
				_playQueueController.processCommand(PlayQueueController::Command::Play, *trackListId);
			});

		bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
			->clicked().connect([=]
			{
				_playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, *trackListId);
			});

		bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
			->clicked().connect([=]
			{
				_playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, *trackListId);
			});

		bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
			->setLink(Wt::WLink {std::make_unique<DownloadTrackListResource>(*trackListId)});

		bindNew<Wt::WPushButton>("delete", Wt::WString::tr("Lms.delete"))
			->clicked().connect([=]
			{
				auto modal {std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.TrackList.template.delete-tracklist"))};
				modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
				Wt::WWidget* modalPtr {modal.get()};

				auto* delBtn {modal->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.delete"))};
				delBtn->clicked().connect([=]
				{
					{
						auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

						Database::TrackList::pointer trackList {Database::TrackList::find(LmsApp->getDbSession(), *trackListId)};
						if (trackList)
							trackList.remove();
					}

					clear();
					trackListDeleted.emit(*trackListId);
					LmsApp->setInternalPath("/tracklists", true);

					LmsApp->getModalManager().dispose(modalPtr);
				});

				auto* cancelBtn {modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel"))};
				cancelBtn->clicked().connect([=]
				{
					LmsApp->getModalManager().dispose(modalPtr);
				});

				LmsApp->getModalManager().show(std::move(modal));
			});

		_container = bindNew<InfiniteScrollingContainer>("tracks", Wt::WString::tr("Lms.Explore.TrackList.template.entry-container"));
		_container->onRequestElements.connect([this]
		{
			addSome();
		});
	}

	void
	TrackList::addSome()
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		Database::Track::FindParameters params;
		params.setClusters(_filters.getClusterIds());
		params.setTrackList(_trackListId);
		params.setSortMethod(Database::TrackSortMethod::TrackList);
		params.setRange({static_cast<std::size_t>(_container->getCount()), _batchSize});
		params.setDistinct(false);

		const auto trackIds {Database::Track::find(LmsApp->getDbSession(), params)};
		for (const TrackId trackId : trackIds.results)
		{
			if (const Track::pointer track {Track::find(LmsApp->getDbSession(), trackId)})
				_container->add(TrackListHelpers::createEntry(track, _playQueueController, _filters));
		}
	}
} // namespace UserInterface

