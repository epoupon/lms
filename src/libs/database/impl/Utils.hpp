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

#include "core/ITraceLogger.hpp"
#include "database/Types.hpp"

namespace lms::db::utils
{
#define ESCAPE_CHAR_STR "\\"
    static inline constexpr char escapeChar{ '\\' };
    std::string escapeLikeKeyword(std::string_view keywords);

    template<typename Query>
    void applyRange(Query& query, std::optional<Range> range)
    {
        if (range)
        {
            query.limit(static_cast<int>(range->size));
            query.offset(static_cast<int>(range->offset));
        }
    }

    template<typename T>
    auto fetchFirstResult(const Wt::Dbo::collection<T>& collection)
    {
        LMS_SCOPED_TRACE_DETAILED("Database", "FetchFirstResult");
        return collection.begin();
    }

    template<typename T>
    void fetchNextResult(typename Wt::Dbo::collection<T>::const_iterator& it)
    {
        LMS_SCOPED_TRACE_DETAILED("Database", "FetchNextResult");
        it++;
    }

    template<typename T, typename Func>
    void forEachResult(const Wt::Dbo::collection<T>& collection, Func&& func)
    {
        typename Wt::Dbo::collection<T>::const_iterator it{ fetchFirstResult(collection) };
        while (it != collection.end())
        {
            func(*it);
            fetchNextResult<T>(it);
        }
    }

    template<typename T>
    struct QueryResultType;

    template<class ResultType, typename BindStrategy>
    struct QueryResultType<Wt::Dbo::Query<ResultType, BindStrategy>>
    {
        using type = ResultType;
    };

    template<typename Query, typename UnaryFunc>
    void forEachQueryResult(const Query& query, UnaryFunc&& func)
    {
        LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "ForEachQueryResult", "Query", query.asString());
        forEachResult(query.resultList(), std::forward<UnaryFunc>(func));
    }

    template<typename T, typename Query>
    std::vector<T> fetchQueryResults(const Query& query)
    {
        LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "FetchQueryResults", "Query", query.asString());

        auto collection{ query.resultList() };
        return std::vector<T>(collection.begin(), collection.end());
    }

    template<typename Query>
    std::vector<typename QueryResultType<Query>::type> fetchQueryResults(const Query& query)
    {
        LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "FetchQueryResults", "Query", query.asString());

        auto collection{ query.resultList() };
        return std::vector<typename QueryResultType<Query>::type>(collection.begin(), collection.end());
    }

    template<typename Query>
    auto fetchQuerySingleResult(const Query& query)
    {
        LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "FetchQuerySingleResult", "Query", query.asString());
        return query.resultValue();
    }

    template<typename ResultType, typename Query>
    RangeResults<ResultType> execRangeQuery(Query& query, const std::optional<Range> range)
    {
        RangeResults<ResultType> res;

        if (range)
        {
            res.range.offset = range->offset;
            applyRange(query, Range{ range->offset, range->size + 1 });

            res.results.reserve(range->size);
        }

        // TODO optim useless last copy
        res.results = utils::fetchQueryResults<ResultType>(query);
        if (range && (res.results.size() == range->size + 1))
        {
            res.moreResults = true;
            res.results.pop_back();
        }

        res.range.size = res.results.size();

        return res;
    }

    template<typename Query, typename UnaryFunc>
    void forEachQueryRangeResult(Query& query, std::optional<Range> range, UnaryFunc&& func)
    {
        if (range)
            applyRange(query, range);

        forEachQueryResult(query, std::forward<UnaryFunc>(func));
    }

    template<typename Query, typename UnaryFunc>
    void forEachQueryRangeResult(Query& query, std::optional<Range> range, bool& moreResults, UnaryFunc&& func)
    {
        using ResultType = typename QueryResultType<Query>::type;

        if (range)
            applyRange(query, Range{ range->offset, range->size + 1 });

        moreResults = false;

        std::size_t count{};
        const auto collection{ query.resultList() };
        auto it{ fetchFirstResult(collection) };
        while (it != collection.end())
        {
            if (range && (count++ == static_cast<std::size_t>(range->size)))
            {
                moreResults = true;
                break;
            }

            func(*it);
            fetchNextResult<ResultType>(it);
        }
    }

    Wt::WDateTime normalizeDateTime(const Wt::WDateTime& dateTime);
} // namespace lms::db::utils