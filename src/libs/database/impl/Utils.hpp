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

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Wt/Dbo/Call.h>
#include <Wt/Dbo/Query.h>
#include <Wt/Dbo/Session.h>
#include <Wt/Dbo/collection.h>
#include <Wt/WDateTime.h>

#include "core/ITraceLogger.hpp"
#include "database/Types.hpp"

#include "DboAccess.hpp"
#include "Session.hpp"
#include "profiling/ScopedQueryProfiler.hpp"

namespace lms::db::utils
{
#define ESCAPE_CHAR_STR "\\"
    static inline constexpr char escapeChar{ '\\' };
    std::string escapeForLikeKeyword(std::string_view keywords);

    Wt::WDateTime normalizeDateTime(const Wt::WDateTime& dateTime);

    template<typename Query>
    void applyRange(Query& query, std::optional<Range> range)
    {
        if (range)
        {
            query.limit(static_cast<int>(range->size));
            if (range->offset != 0)
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

    // Mapped entities must be fetched using the *Object variants, so that callers get ObjectPtr
    template<typename T>
    inline constexpr bool isObjectResult{ false };

    template<MappedEntity DboType>
    inline constexpr bool isObjectResult<Wt::Dbo::ptr<DboType>>{ true };

    namespace detail
    {
        template<typename Query, typename UnaryFunc>
        void forEachQueryResult(const Query& query, UnaryFunc&& func)
        {
            LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "ForEachQueryResult", "Query", query.asString());

            ScopedQueryProfiler queryProfiler{ query };
            forEachResult(query.resultList(), [&](const auto& result) {
                queryProfiler.suspend();
                func(result);
                queryProfiler.resume();
            });
        }
    } // namespace detail

    template<typename Query, typename UnaryFunc>
    void forEachQueryResult(const Query& query, UnaryFunc&& func)
    {
        static_assert(!isObjectResult<typename QueryResultType<Query>::type>, "Use forEachQueryObject");
        detail::forEachQueryResult(query, std::forward<UnaryFunc>(func));
    }

    template<typename Query, typename UnaryFunc>
    void forEachQueryObject(const Query& query, UnaryFunc&& func)
    {
        detail::forEachQueryResult(query, [&](const auto& result) {
            func(toObjectPtr(result));
        });
    }

    template<typename T, typename Query>
    std::vector<T> fetchQueryResults(const Query& query)
    {
        static_assert(!isObjectResult<typename QueryResultType<Query>::type>, "Use fetchQueryObjects");
        LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "FetchQueryResults", "Query", query.asString());

        ScopedQueryProfiler queryProfiler{ query };
        auto collection{ query.resultList() };
        return std::vector<T>(collection.begin(), collection.end());
    }

    template<typename Query>
    std::vector<typename QueryResultType<Query>::type> fetchQueryResults(const Query& query)
    {
        return fetchQueryResults<typename QueryResultType<Query>::type>(query);
    }

    template<typename Query>
    auto fetchQueryObjects(const Query& query)
    {
        LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "FetchQueryResults", "Query", query.asString());

        ScopedQueryProfiler queryProfiler{ query };
        std::vector<decltype(toObjectPtr(std::declval<typename QueryResultType<Query>::type>()))> results;
        for (const auto& result : query.resultList())
            results.push_back(toObjectPtr(result));

        return results;
    }

    template<typename Query>
    auto fetchQuerySingleResult(const Query& query)
    {
        static_assert(!isObjectResult<typename QueryResultType<Query>::type>, "Use fetchQuerySingleObject");
        LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "FetchQuerySingleResult", "Query", query.asString());
        ScopedQueryProfiler queryProfiler{ query };
        return query.resultValue();
    }

    template<typename Query>
    auto fetchQuerySingleObject(const Query& query)
    {
        LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "FetchQuerySingleResult", "Query", query.asString());
        ScopedQueryProfiler queryProfiler{ query };
        return toObjectPtr(query.resultValue());
    }

    template<typename ResultType, typename Query>
    std::vector<ResultType> execRangeQuery(Query& query, const std::optional<Range> range)
    {
        applyRange(query, range);
        return utils::fetchQueryResults<ResultType>(query);
    }

    template<typename Query>
    auto execRangeObjectQuery(Query& query, const std::optional<Range> range)
    {
        applyRange(query, range);
        return utils::fetchQueryObjects(query);
    }

    template<typename Query, typename UnaryFunc>
    void forEachQueryRangeResult(Query& query, std::optional<Range> range, UnaryFunc&& func)
    {
        if (range)
            applyRange(query, range);

        forEachQueryResult(query, std::forward<UnaryFunc>(func));
    }

    template<typename Query, typename UnaryFunc>
    void forEachQueryRangeObject(Query& query, std::optional<Range> range, UnaryFunc&& func)
    {
        if (range)
            applyRange(query, range);

        forEachQueryObject(query, std::forward<UnaryFunc>(func));
    }

    template<typename... Args>
    void executeCommand(Wt::Dbo::Session& session, std::string_view command, const Args&... args)
    {
        Wt::Dbo::Call call{ session.execute(std::string{ command }) };
        (call.bind(args), ...);

        {
            LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Database", "ExecuteCommand", "Command", command);
            call.run();
        }
    }

    template<typename Object>
    typename Object::pointer addObject(ISession& session, std::unique_ptr<Object> object)
    {
        checkWriteTransaction(session);

        Wt::Dbo::Session& dboSession{ *getDboSession(session) };
        typename Object::pointer res{ toObjectPtr(dboSession.add(std::move(object))) };
        dboSession.flush();

        return res;
    }

    template<typename Object>
    void destroyObjects(ISession& session, std::span<const typename Object::IdType> ids)
    {
        checkWriteTransaction(session);

        Wt::Dbo::Session& dboSession{ *getDboSession(session) };
        const std::string query{ std::string{ "DELETE FROM " } + dboSession.tableName<Object>() + " WHERE id = ?" };
        for (typename Object::IdType id : ids)
            executeCommand(dboSession, query, id.getValue());
    }
} // namespace lms::db::utils