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

#include "DatabaseCollectorBase.hpp"

#include <algorithm>

#include "core/String.hpp"

#include "explore/Filters.hpp"

namespace lms::ui
{
    DatabaseCollectorBase::DatabaseCollectorBase(Filters& filters, Mode defaultMode, std::size_t maxCount)
        : _filters{ filters }
        , _mode{ defaultMode }
        , _maxCount{ maxCount }
    {
    }

    DatabaseCollectorBase::Range DatabaseCollectorBase::getActualRange(std::optional<db::Range> requestedRange) const
    {
        db::Range res;

        if (!requestedRange)
        {
            res.offset = 0;
            res.size = _maxCount;
        }
        else
        {
            res.offset = requestedRange->offset;
            if (requestedRange->offset < _maxCount)
                res.size = std::min(_maxCount - requestedRange->offset, requestedRange->size);
            else
                res.size = 0;
        }

        return res;
    }

    std::size_t DatabaseCollectorBase::getMaxCount() const
    {
        return _maxCount;
    }

    const db::Filters& DatabaseCollectorBase::getDbFilters() const
    {
        return _filters.getDbFilters();
    }

    void DatabaseCollectorBase::setSearch(std::string_view searchText)
    {
        _searchText = searchText;
        if (!searchText.empty())
            _searchKeywords = core::stringUtils::splitString(_searchText, ' ');
        else
            _searchKeywords.clear();
    }

} // namespace lms::ui
