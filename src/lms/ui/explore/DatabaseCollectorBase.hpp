/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "database/Types.hpp"

namespace lms::db
{
    struct Filters;
}

namespace lms::ui
{
    class Filters;

    class DatabaseCollectorBase
    {
    public:
        using Range = db::Range;

        virtual ~DatabaseCollectorBase() {}

        enum class Mode
        {
            Random,
            Starred,
            RecentlyPlayed,
            RecentlyAdded,
            RecentlyModified,
            MostPlayed,
            All
        };

        DatabaseCollectorBase(Filters& filters, Mode defaultMode, std::size_t maxCount);

        Mode getMode() const { return _mode; }
        void setMode(Mode mode) { _mode = mode; }
        void setSearch(std::string_view search);

    protected:
        Range getActualRange(std::optional<Range> range) const;
        std::size_t getMaxCount() const;
        const db::Filters& getDbFilters() const;
        const std::vector<std::string_view>& getSearchKeywords() const { return _searchKeywords; }

    private:
        Filters& _filters;
        std::string _searchText;
        std::vector<std::string_view> _searchKeywords;
        Mode _mode;
        std::size_t _maxCount;
    };
} // namespace lms::ui
