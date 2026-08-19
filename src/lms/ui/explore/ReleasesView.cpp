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

#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>

#include "database/Session.hpp"
#include "database/objects/Release.hpp"

#include "LmsApplication.hpp"
#include "SortModeSelector.hpp"
#include "State.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "common/Template.hpp"
#include "explore/Filters.hpp"
#include "explore/PlayQueueController.hpp"
#include "explore/ReleaseHelpers.hpp"

namespace lms::ui
{
    Releases::Releases(Filters& filters, PlayQueueController& playQueueController)
        : Template{ Wt::WString::tr("Lms.Explore.Releases.template") }
        , _playQueueController{ playQueueController }
        , _releaseCollector{ filters, _defaultMode, _maxCount }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        Wt::WLineEdit* searEdit{ bindNew<Wt::WLineEdit>("search") };
        searEdit->setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder"));
        searEdit->textInput().connect([this, searEdit] {
            refreshView(searEdit->text());
        });

        {
            const ReleaseCollector::Mode sortMode{ state::readValue<ReleaseCollector::Mode>("releases_sort_mode").value_or(_defaultMode) };
            _releaseCollector.setMode(sortMode);

            SortModeSelector* sortModeSelector{ bindNew<SortModeSelector>("sort-mode", sortMode) };
            sortModeSelector->itemSelected.connect([this](ReleaseCollector::Mode newSortMode) {
                state::writeValue<ReleaseCollector::Mode>("releases_sort_mode", newSortMode);
                refreshView(newSortMode);
            });
        }

        Wt::WPushButton* playBtn{ bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML) };
        playBtn->clicked().connect([this] {
            _playQueueController.processCommand(PlayQueueController::Command::Play, getAllReleases());
        });

        bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, getAllReleases());
            });
        bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, getAllReleases());
            });
        bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, getAllReleases());
            });

        _container = bindNew<InfiniteScrollingContainer>("releases", Wt::WString::tr("Lms.Explore.Releases.template.container"));
        _container->onRequestElements.connect([this] {
            addSome();
        });

        filters.updated().connect([this] {
            refreshView();
        });

        refreshView(_releaseCollector.getMode());
    }

    void Releases::refreshView()
    {
        _container->reset();
        _releaseCollector.reset();
    }

    void Releases::refreshView(ReleaseCollector::Mode mode)
    {
        _releaseCollector.setMode(mode);
        refreshView();
    }

    void Releases::refreshView(const Wt::WString& searchText)
    {
        _releaseCollector.setSearch(searchText.toUTF8());
        refreshView();
    }

    void Releases::addSome()
    {
        bool moreResults{};
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };
            _releaseCollector.get(db::Range{ static_cast<std::size_t>(_container->getCount()), _batchSize }, moreResults, [&](const db::Release::pointer& release) {
                _container->add(releaseListHelpers::createEntry(release, { releaseListHelpers::DisplayOptions::ShowArtist }));
            });
        }
        _container->setHasMore(moreResults);
    }

    std::vector<db::ReleaseId> Releases::getAllReleases()
    {
        std::vector<db::ReleaseId> ids;
        bool moreResults{};
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };
            _releaseCollector.get(db::Range{ 0, _maxCount }, moreResults, [&](const db::Release::pointer& release) {
                ids.push_back(release->getId());
            });
        }
        return ids;
    }

} // namespace lms::ui
