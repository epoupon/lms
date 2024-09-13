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

#pragma once

#include <unordered_map>

#include <Wt/WTemplate.h>

#include "database/Object.hpp"
#include "database/TrackListId.hpp"
#include "database/Types.hpp"

#include "common/Template.hpp"

namespace lms::db
{
    class TrackList;
}

namespace lms::ui
{
    class Filters;
    class InfiniteScrollingContainer;

    class TrackLists : public Template
    {
    public:
        TrackLists(Filters& filters);

        void onTrackListDeleted(db::TrackListId trackListId);

    private:
        enum class Mode
        {
            RecentlyModified,
            All,
        };

        void refreshView();
        void addSome();
        void addTracklist(const db::ObjectPtr<db::TrackList>& trackList);

        static constexpr Mode _defaultMode{ Mode::RecentlyModified };
        static constexpr std::size_t _batchSize{ 30 };
        static constexpr std::size_t _maxCount{ 500 };

        Mode _mode;
        Filters& _filters;
        Wt::WWidget* _currentActiveItem{};
        InfiniteScrollingContainer* _container{};
        std::unordered_map<db::TrackListId, Wt::WWidget*> _trackListWidgets;
    };
} // namespace lms::ui
