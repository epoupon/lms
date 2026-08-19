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

#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/TrackArtistLink.hpp"

#include "ArtistListHelpers.hpp"
#include "ArtistTypeSelector.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "SortModeSelector.hpp"
#include "State.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "database/objects/Types.hpp"
#include "explore/ArtistType.hpp"

namespace lms::ui
{
    Artists::Artists(Filters& filters)
        : Template{ Wt::WString::tr("Lms.Explore.Artists.template") }
        , _artistCollector{ filters, _defaultSortMode, _maxCount }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        Wt::WLineEdit* searchEdit{ bindNew<Wt::WLineEdit>("search") };
        searchEdit->setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder"));
        searchEdit->textInput().connect([this, searchEdit] {
            refreshView(searchEdit->text());
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
            ArtistType artistType{ state::readValue<ArtistType>("artists_type").value_or(_defaultArtistType) };
            _artistCollector.setArtistType(artistType);

            ArtistTypeSelector* artistTypeSelector{ bindNew<ArtistTypeSelector>("artist-type", artistType) };
            artistTypeSelector->itemSelected.connect([this](ArtistType newArtistType) {
                state::writeValue<ArtistType>("artists_type", newArtistType);
                refreshView(newArtistType);
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

    void Artists::refreshView(ArtistType artistType)
    {
        _artistCollector.setArtistType(artistType);
        refreshView();
    }

    void Artists::refreshView(const Wt::WString& searchText)
    {
        _artistCollector.setSearch(searchText.toUTF8());
        refreshView();
    }

    void Artists::addSome()
    {
        bool moreResults{};
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };
            _artistCollector.get(db::Range{ static_cast<std::size_t>(_container->getCount()), _batchSize }, moreResults, [&](const db::Artist::pointer& artist) {
                _container->add(ArtistListHelpers::createEntry(artist));
            });
        }
        _container->setHasMore(moreResults);
    }

} // namespace lms::ui
