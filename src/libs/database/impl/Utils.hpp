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
#include "core/ITraceLogger.hpp"

namespace lms::db::utils
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

    template <typename Query>
    auto execSingleResultQuery(const Query& query)
    {
        LMS_SCOPED_TRACE_DETAILED("Database", "ExecSingleResultQuery");
        return query.resultValue();
    }

    template <typename Query>
    auto execMultiResultQuery(const Query& query)
    {
        LMS_SCOPED_TRACE_DETAILED("Database", "ExecMultiResultQuery");
        return query.resultList();
    }

    template <typename ResultType, typename Query>
    RangeResults<ResultType> execRangeQuery(Query& query, std::optional<Range> range)
    {
        RangeResults<ResultType> res;

        if (range)
        {
            res.range.offset = range->offset;
            applyRange(query, Range{ range->offset, range->size + 1 });

            res.results.reserve(range->size);
        }

        auto collection{ execMultiResultQuery(query) };
        for (auto itResult{ collection.begin() }; itResult != collection.end(); ++itResult)
        {
            if (range && res.results.size() == range->size)
            {
                res.moreResults = true;
                break;
            }

            res.results.push_back(std::move(*itResult));
        }

        res.range.size = res.results.size();

        return res;
    }

    template <typename ResultType, typename Query>
    void execRangeQuery(Query& query, std::optional<Range> range, const std::function<void(const ResultType&)>& func)
    {
        if (range)
            applyRange(query, range);

        auto collection{ execMultiResultQuery(query) };
        for (const auto& res : collection)
        {
            LMS_SCOPED_TRACE_DETAILED("Database", "ExecQueryRangeForEach");
            func(res);
        }
    }

    template <typename ResultType, typename Query>
    void execRangeQuery(Query& query, std::optional<Range> range, bool& moreResults, const std::function<void(const ResultType&)>& func)
    {
        if (range)
            applyRange(query, Range{ range->offset, range->size + 1 });

        moreResults = false;

        std::size_t count{};
        auto collection{ execMultiResultQuery(query) };
        for (const auto& res : collection)
        {
            if (range && (count++ == static_cast<std::size_t>(range->size)))
            {
                moreResults = true;
                break;
            }

            LMS_SCOPED_TRACE_DETAILED("Database", "ExecQueryRangeForEach");
            func(res);
        }
    }

    Wt::WDateTime normalizeDateTime(const Wt::WDateTime& dateTime);
}

