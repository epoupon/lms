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

#include "ArtistsView.hpp"

#include <Wt/WPushButton.h>

#include "core/ILogger.hpp"
#include "database/Artist.hpp"
#include "database/Session.hpp"
#include "database/TrackArtistLink.hpp"

#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "SortModeSelector.hpp"
#include "State.hpp"
#include "TrackArtistLinkTypeSelector.hpp"
#include "common/InfiniteScrollingContainer.hpp"

namespace lms::ui
{
    using namespace db;

    Artists::Artists(Filters& filters)
        : Template{ Wt::WString::tr("Lms.Explore.Artists.template") }
        , _artistCollector{ filters, _defaultSortMode, _maxCount }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        Wt::WLineEdit* searEdit{ bindNew<Wt::WLineEdit>("search") };
        searEdit->setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder"));
        searEdit->textInput().connect([this, searEdit] {
            refreshView(searEdit->text());
        });

        {
            const ArtistCollector::Mode sortMode{ state::readValue<ArtistCollector::Mode>("artists_sort_mode").value_or(_defaultSortMode) };
            _artistCollector.setMode(sortMode);

            SortModeSelector* sortModeSelector{ bindNew<SortModeSelector>("sort-mode", sortMode) };
            sortModeSelector->itemSelected.connect([this](ArtistCollector::Mode newSortMode) {
                state::writeValue<ArtistCollector::Mode>("artists_sort_mode", newSortMode);
                refreshView(newSortMode);
            });
        }

        {
            const std::optional<TrackArtistLinkType> linkType{ state::readValue<TrackArtistLinkType>("artists_link_type") };
            _artistCollector.setArtistLinkType(linkType);

            TrackArtistLinkTypeSelector* linkTypeSelector{ bindNew<TrackArtistLinkTypeSelector>("link-type", linkType) };
            linkTypeSelector->itemSelected.connect([this](std::optional<TrackArtistLinkType> newLinkType) {
                state::writeValue<TrackArtistLinkType>("artists_link_type", newLinkType);
                refreshView(newLinkType);
            });
        }

        _container = bindNew<InfiniteScrollingContainer>("artists", Wt::WString::tr("Lms.Explore.Artists.template.container"));
        _container->onRequestElements.connect([this] {
            addSome();
        });

        filters.updated().connect([this] {
            refreshView();
        });

        refreshView(_artistCollector.getMode());
    }

    void Artists::refreshView()
    {
        _container->reset();
        _artistCollector.reset();
    }

    void Artists::refreshView(ArtistCollector::Mode mode)
    {
        _artistCollector.setMode(mode);
        refreshView();
    }

    void Artists::refreshView(std::optional<TrackArtistLinkType> linkType)
    {
        _artistCollector.setArtistLinkType(linkType);
        refreshView();
    }

    void Artists::refreshView(const Wt::WString& searchText)
    {
        _artistCollector.setSearch(searchText.toUTF8());
        refreshView();
    }

    void Artists::addSome()
    {
        const auto artistIds{ _artistCollector.get(Range{ static_cast<std::size_t>(_container->getCount()), _batchSize }) };

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            for (const ArtistId artistId : artistIds.results)
            {
                if (const auto artist{ Artist::find(LmsApp->getDbSession(), artistId) })
                    _container->add(ArtistListHelpers::createEntry(artist));
            }
        }

        _container->setHasMore(artistIds.moreResults);
    }

} // namespace lms::ui
