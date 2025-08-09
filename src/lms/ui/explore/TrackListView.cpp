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

#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"

#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "ModalManager.hpp"
#include "Utils.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "explore/Filters.hpp"
#include "explore/PlayQueueController.hpp"
#include "explore/TrackListHelpers.hpp"
#include "resource/DownloadResource.hpp"

namespace lms::ui
{
    namespace
    {
        std::optional<db::TrackListId> extractTrackListIdFromInternalPath()
        {
            return core::stringUtils::readAs<db::TrackListId::ValueType>(wApp->internalPathNextPart("/tracklist/"));
        }
    } // namespace

    TrackList::TrackList(Filters& filters, PlayQueueController& playQueueController)
        : Template{ Wt::WString::tr("Lms.Explore.TrackList.template") }
        , _filters{ filters }
        , _playQueueController{ playQueueController }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        _filters.updated().connect([this] {
            refreshView();
        });

        refreshView();
    }

    void TrackList::refreshView()
    {
        if (!wApp->internalPathMatches("/tracklist/"))
            return;

        const std::optional<db::TrackListId> trackListId{ extractTrackListIdFromInternalPath() };
        if (!trackListId)
            throw TrackListNotFoundException{};

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::TrackList::pointer trackList{ db::TrackList::find(LmsApp->getDbSession(), *trackListId) };
        if (!trackList || trackList->getType() != db::TrackListType::PlayList)
            throw TrackListNotFoundException{};

        if (trackList->getUserId().isValid()
            && LmsApp->getUserId() != trackList->getUserId()
            && trackList->getVisibility() != db::TrackList::Visibility::Public)
        {
            throw TrackListNotFoundException{};
        }

        LmsApp->setTitle(std::string{ trackList->getName() });
        _trackListId = *trackListId;

        clear();

        bindString("name", std::string{ trackList->getName() }, Wt::TextFormat::Plain);
        bindString("duration", utils::durationToString(trackList->getDuration()));
        const auto trackCount{ trackList->getCount() };
        bindString("track-count", Wt::WString::trn("Lms.track-count", trackCount).arg(trackCount));

        Wt::WContainerWidget* clusterContainers{ bindNew<Wt::WContainerWidget>("clusters") };
        {
            const auto clusterTypeIds{ db::ClusterType::findIds(LmsApp->getDbSession()).results };
            const auto clusterGroups{ trackList->getClusterGroups(clusterTypeIds, 3) };

            for (const auto& clusters : clusterGroups)
            {
                for (const db::Cluster::pointer& cluster : clusters)
                {
                    const db::ClusterId clusterId{ cluster->getId() };
                    Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterCluster(clusterId)) };
                    entry->clicked().connect([this, clusterId] {
                        _filters.add(clusterId);
                    });
                }
            }
        }

        bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
            ->clicked()
            .connect([this, trackListId] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, *trackListId);
            });

        bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this, trackListId] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, *trackListId);
            });

        bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this, trackListId] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, *trackListId);
            });

        bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
            ->setLink(Wt::WLink{ std::make_unique<DownloadTrackListResource>(*trackListId) });

        if (trackList->getUserId() == LmsApp->getUserId())
        {
            setCondition("if-has-delete", true);
            bindNew<Wt::WPushButton>("delete", Wt::WString::tr("Lms.delete"))
                ->clicked()
                .connect([this, trackListId] {
                    auto modal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.TrackList.template.delete-tracklist")) };
                    modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
                    Wt::WWidget* modalPtr{ modal.get() };

                    auto* delBtn{ modal->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.delete")) };
                    delBtn->clicked().connect([this, trackListId, modalPtr] {
                        {
                            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                            db::TrackList::pointer trackList{ db::TrackList::find(LmsApp->getDbSession(), *trackListId) };
                            if (trackList)
                                trackList.remove();
                        }

                        clear();
                        trackListDeleted.emit(*trackListId);
                        LmsApp->setInternalPath("/tracklists", true);

                        LmsApp->getModalManager().dispose(modalPtr);
                    });

                    auto* cancelBtn{ modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel")) };
                    cancelBtn->clicked().connect([=] {
                        LmsApp->getModalManager().dispose(modalPtr);
                    });

                    LmsApp->getModalManager().show(std::move(modal));
                });
        }

        _container = bindNew<InfiniteScrollingContainer>("tracks", Wt::WString::tr("Lms.Explore.TrackList.template.entry-container"));
        _container->onRequestElements.connect([this] {
            addSome();
        });
    }

    void TrackList::addSome()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        db::Track::FindParameters params;
        params.setFilters(_filters.getDbFilters());
        params.setTrackList(_trackListId);
        params.setSortMethod(db::TrackSortMethod::TrackList);
        params.setRange(db::Range{ static_cast<std::size_t>(_container->getCount()), _batchSize });

        bool moreResults{};
        db::Track::find(LmsApp->getDbSession(), params, moreResults, [this](const db::Track::pointer& track) {
            _container->add(TrackListHelpers::createEntry(track, _playQueueController, _filters));
        });

        _container->setHasMore(moreResults);
    }
} // namespace lms::ui
