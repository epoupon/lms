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

#include "QueryPlanRecorder.hpp"

#include <memory>
#include <mutex>

#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/Transaction.h>

#include "core/ILogger.hpp"

namespace lms::db
{
    std::unique_ptr<IQueryPlanRecorder> createQueryPlanRecorder()
    {
        return std::make_unique<QueryPlanRecorder>();
    }

    QueryPlanRecorder::QueryPlanRecorder()
    {
        LMS_LOG(DB, INFO, "Recording database query plans");
    }

    QueryPlanRecorder::~QueryPlanRecorder() = default;

    void QueryPlanRecorder::visitQueryPlans(const QueryPlanVisitor& visitor) const
    {
        const std::shared_lock lock{ _mutex };

        for (const auto& [query, plan] : _queryPlans)
            visitor(query, plan);
    }

    void QueryPlanRecorder::recordQueryPlanIfNeeded(Wt::Dbo::Session& session, const std::string& query)
    {
        {
            const std::shared_lock lock{ _mutex };

            if (_queryPlans.contains(query))
                return;
        }

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
            _queryPlans.try_emplace(query, std::move(result));
        }
    }
} // namespace lms::db
