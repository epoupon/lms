/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "profiling/QueryProfiler.hpp"

#include <memory>
#include <mutex>

#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/Transaction.h>

#include "core/ILogger.hpp"

namespace lms::db
{
    std::unique_ptr<IQueryProfiler> createQueryProfiler()
    {
        return std::make_unique<QueryProfiler>();
    }

    QueryProfiler::QueryProfiler()
    {
        LMS_LOG(DB, INFO, "Recording database queries");
    }

    QueryProfiler::~QueryProfiler() = default;

    void QueryProfiler::visitQueries(const QueryVisitor& visitor) const
    {
        const std::shared_lock lock{ _mutex };

        for (const auto& [query, data] : _queries)
        {
            const QueryStats stats{
                .query = query,
                .plan = data.plan,
                .callCount = data.timeStats.getCount(),
                .totalTime = std::chrono::microseconds{ static_cast<long long>(data.timeStats.getMean() * static_cast<double>(data.timeStats.getCount())) },
                .meanTime = std::chrono::microseconds{ static_cast<long long>(data.timeStats.getMean()) },
                .stdDevTime = std::chrono::microseconds{ static_cast<long long>(data.timeStats.getSampleStdDev()) },
            };
            visitor(stats);
        }
    }

    void QueryProfiler::recordQueryPlan(Wt::Dbo::Session& session, const std::string& query)
    {
        Wt::Dbo::Transaction transaction{ session };

        Wt::Dbo::SqlConnection* connection{ transaction.connection() };
        auto statement{ connection->prepareStatement("EXPLAIN QUERY PLAN " + query) };
        statement->execute();

        std::map<int, std::string> entries{ { 0, "" } };
        std::map<int, std::vector<int>> relationships;

        std::string detail;
        while (statement->nextRow())
        {
            detail.clear();

            int id{};
            int parent{};
            int unused{};

            if (statement->getResult(0, &id)
                && statement->getResult(1, &parent)
                && statement->getResult(2, &unused)
                && statement->getResult(3, &detail, static_cast<int>(detail.capacity())))
            {
                entries.emplace(id, detail);
                relationships[parent].push_back(id);
            }
        }

        // format
        std::string result;
        std::function<void(int, unsigned)> formatQuery = [&](int id, unsigned level) -> void {
            for (std::size_t i{}; i < level; ++i)
                result += '\t';

            result += entries.at(id);
            result += '\n';
            auto itChildren = relationships.find(id);
            if (itChildren == relationships.end())
                return;

            for (int child : itChildren->second)
                formatQuery(child, level + 1);
        };

        formatQuery(0, 0);

        {
            const std::unique_lock lock{ _mutex };
            _queries[query].plan = std::move(result);
        }
    }

    void QueryProfiler::recordQueryExecution(Wt::Dbo::Session& session, const std::string& query, Clock::duration elapsed)
    {
        bool needQueryPlan{};
        const double elapsedUs{ std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(elapsed).count() };
        {
            std::unique_lock lock{ _mutex };

            auto& queryStats{ _queries[query] };
            queryStats.timeStats.add(elapsedUs);
            needQueryPlan = queryStats.plan.empty();
        }

        if (needQueryPlan)
            recordQueryPlan(session, query);
    }
} // namespace lms::db
