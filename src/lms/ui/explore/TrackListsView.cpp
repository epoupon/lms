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

#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>

#include "database/Session.hpp"
#include "database/objects/TrackList.hpp"

#include "DropDownMenuSelector.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "State.hpp"
#include "Utils.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "common/Template.hpp"

namespace lms::ui
{
    TrackLists::TrackLists(Filters& filters)
        : Template{ Wt::WString::tr("Lms.Explore.TrackLists.template") }
        , _filters{ filters }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        {
            _type = state::readValue<Type>("tracklists_type").value_or(_defaultType);

            using TypeSelector = DropDownMenuSelector<Type>;
            TypeSelector* typeSelector{ bindNew<TypeSelector>("tracklist-type", Wt::WString::tr("Lms.Explore.TrackLists.template.type-selector"), _type) };
            typeSelector->bindItem("owned", Wt::WString::tr("Lms.Explore.TrackLists.type-owned"), Type::Owned);
            typeSelector->bindItem("shared", Wt::WString::tr("Lms.Explore.TrackLists.type-shared"), Type::Shared);

            typeSelector->itemSelected.connect(this, [this](Type type) {
                state::writeValue<Type>("tracklists_type", type);
                _type = type;
                refreshView();
            });
        }

        Wt::WLineEdit* searchEdit{ bindNew<Wt::WLineEdit>("search") };
        searchEdit->setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder"));
        searchEdit->textInput().connect([this, searchEdit] {
            _searchText = searchEdit->text().toUTF8();
            refreshView();
        });

        {
            _sortMode = state::readValue<SortMode>("tracklists_sort_mode").value_or(_defaultSortMode);

            using SortModeSelector = DropDownMenuSelector<SortMode>;
            SortModeSelector* sortModeSelector{ bindNew<SortModeSelector>("sort-mode", Wt::WString::tr("Lms.Explore.TrackLists.template.sort-mode"), _sortMode) };
            sortModeSelector->bindItem("recently-modified", Wt::WString::tr("Lms.Explore.recently-modified"), SortMode::RecentlyModified);
            sortModeSelector->bindItem("all", Wt::WString::tr("Lms.Explore.all"), SortMode::All);

            sortModeSelector->itemSelected.connect(this, [this](SortMode mode) {
                state::writeValue<SortMode>("tracklists_sort_mode", mode);
                _sortMode = mode;
                refreshView();
            });
        }

        _container = bindNew<InfiniteScrollingContainer>("tracklists", Wt::WString::tr("Lms.Explore.TrackLists.template.container"));
        _container->onRequestElements.connect([this] {
            addSome();
        });

        _filters.updated().connect([this] {
            refreshView();
        });

        refreshView();
    }

    void TrackLists::onTrackListDeleted(db::TrackListId trackListId)
    {
        auto itTrackList{ _trackListWidgets.find(trackListId) };
        if (itTrackList == std::end(_trackListWidgets))
            return;

        _container->remove(*itTrackList->second);
        _trackListWidgets.erase(itTrackList);
    }

    void TrackLists::refreshView()
    {
        _container->reset();
        _trackListWidgets.clear();
    }

    void TrackLists::addSome()
    {
        const db::Range range{ static_cast<std::size_t>(_container->getCount()), _batchSize };

        db::Session& session{ LmsApp->getDbSession() };
        auto transaction{ session.createReadTransaction() };

        db::TrackList::FindParameters params;

        if (!_searchText.empty())
            params.setKeywords(core::stringUtils::splitString(_searchText, ' '));
        params.setFilters(_filters.getDbFilters());
        params.setType(db::TrackListType::PlayList);
        params.setRange(range);

        switch (_type)
        {
        case Type::Owned:
            params.setUser(LmsApp->getUserId());
            break;

        case Type::Shared:
            params.setExcludedUser(LmsApp->getUserId());
            params.setVisibility(db::TrackList::Visibility::Public);
            break;
        }

        switch (_sortMode)
        {
        case SortMode::All:
            params.setSortMethod(db::TrackListSortMethod::Name);
            break;
        case SortMode::RecentlyModified:
            params.setSortMethod(db::TrackListSortMethod::LastModifiedDesc);
            break;
        }

        const auto trackListIds{ db::TrackList::find(session, params) };
        for (const db::TrackListId trackListId : trackListIds.results)
        {
            if (const db::TrackList::pointer trackList{ db::TrackList::find(LmsApp->getDbSession(), trackListId) })
                addTracklist(trackList);
        }

        _container->setHasMore(trackListIds.moreResults);
    }

    void TrackLists::addTracklist(const db::ObjectPtr<db::TrackList>& trackList)
    {
        const db::TrackListId trackListId{ trackList->getId() };

        WTemplate* entry{ _container->addNew<Template>(Wt::WString::tr("Lms.Explore.TrackLists.template.entry")) };
        entry->bindWidget("name", utils::createTrackListAnchor(trackList));

        assert(_trackListWidgets.find(trackListId) == std::cend(_trackListWidgets));
        _trackListWidgets.emplace(trackListId, entry);
    }

} // namespace lms::ui
