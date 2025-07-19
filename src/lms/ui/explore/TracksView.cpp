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

#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>

#include "database/Session.hpp"
#include "database/objects/Track.hpp"

#include "LmsApplication.hpp"
#include "SortModeSelector.hpp"
#include "State.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "explore/Filters.hpp"
#include "explore/PlayQueueController.hpp"
#include "explore/TrackListHelpers.hpp"

namespace lms::ui
{
    using namespace db;

    Tracks::Tracks(Filters& filters, PlayQueueController& playQueueController)
        : Template{ Wt::WString::tr("Lms.Explore.Tracks.template") }
        , _filters{ filters }
        , _playQueueController{ playQueueController }
        , _trackCollector{ filters, _defaultMode, _maxCount }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        Wt::WLineEdit* searEdit{ bindNew<Wt::WLineEdit>("search") };
        searEdit->setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder"));
        searEdit->textInput().connect([this, searEdit] {
            refreshView(searEdit->text());
        });

        {
            const TrackCollector::Mode sortMode{ state::readValue<TrackCollector::Mode>("tracks_sort_mode").value_or(_defaultMode) };
            _trackCollector.setMode(sortMode);

            SortModeSelector* sortModeSelector{ bindNew<SortModeSelector>("sort-mode", sortMode) };
            sortModeSelector->itemSelected.connect([this](TrackCollector::Mode newSortMode) {
                state::writeValue<TrackCollector::Mode>("tracks_sort_mode", newSortMode);
                refreshView(newSortMode);
            });
        }

        bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, getAllTracks());
            });

        bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, getAllTracks());
            });
        bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, getAllTracks());
            });
        bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, getAllTracks());
            });

        _container = bindNew<InfiniteScrollingContainer>("tracks", Wt::WString::tr("Lms.Explore.Tracks.template.entry-container"));
        _container->onRequestElements.connect([this] {
            addSome();
        });

        filters.updated().connect([this] {
            refreshView();
        });

        refreshView(_trackCollector.getMode());
    }

    void Tracks::refreshView()
    {
        _container->reset();
        _trackCollector.reset();
    }

    void Tracks::refreshView(TrackCollector::Mode mode)
    {
        _trackCollector.setMode(mode);
        refreshView();
    }

    void Tracks::refreshView(const Wt::WString& searchText)
    {
        _trackCollector.setSearch(searchText.toUTF8());
        refreshView();
    }

    void Tracks::addSome()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const auto trackIds{ _trackCollector.get(Range{ static_cast<std::size_t>(_container->getCount()), _batchSize }) };

        for (const TrackId trackId : trackIds.results)
        {
            if (const Track::pointer track{ Track::find(LmsApp->getDbSession(), trackId) })
                _container->add(TrackListHelpers::createEntry(track, _playQueueController, _filters));
        }

        _container->setHasMore(trackIds.moreResults);
    }

    std::vector<db::TrackId> Tracks::getAllTracks()
    {
        RangeResults<TrackId> trackIds{ _trackCollector.get() };
        return std::move(trackIds.results);
    }
} // namespace lms::ui
