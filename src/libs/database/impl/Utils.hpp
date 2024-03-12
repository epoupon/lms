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

#include <functional>
#include <string>
#include <string_view>

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "database/Types.hpp"
#include "utils/ITraceLogger.hpp"

namespace Database::Utils
{
#define ESCAPE_CHAR_STR "\\"
    static inline constexpr char escapeChar{ '\\' };
    std::string escapeLikeKeyword(std::string_view keywords);

    template <typename Query>
    void applyRange(Query& query, std::optional<Range> range)
    {
        if (range)
        {
            query.limit(static_cast<int>(range->size));
            query.offset(static_cast<int>(range->offset));
        }
    }

    template <typename ResultType, typename Query>
    RangeResults<ResultType> execQuery(Query& query, std::optional<Range> range)
    {
        LMS_SCOPED_TRACE_DETAILED("Database", "ExecQueryRange");

        RangeResults<ResultType> res;

        if (range)
        {
            res.range.offset = range->offset;
            applyRange(query, Range{ range->offset, range->size + 1 });
        }

        auto collection{ query.resultList() };
        res.results.assign(collection.begin(), collection.end());
        if (range && res.results.size() == static_cast<std::size_t>(range->size) + 1)
        {
            // TODO may optim by not actually requesting the last one
            res.moreResults = true;
            res.results.pop_back();
        }

        res.range.size = res.results.size();

        return res;
    }

    template <typename ResultType, typename Query>
    void execQuery(Query& query, std::optional<Range> range, std::function<void(const ResultType&)> func)
    {
        if (range)
            applyRange(query, range);

        for (const auto& res : query.resultList())
        {
            LMS_SCOPED_TRACE_DETAILED("Database", "ExecQueryResult");
            func(res);
        }
    }

    Wt::WDateTime normalizeDateTime(const Wt::WDateTime& dateTime);
} // namespace Database::Utils

