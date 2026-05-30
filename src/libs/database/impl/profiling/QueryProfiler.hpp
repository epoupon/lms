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

#pragma once

#include <map>
#include <shared_mutex>
#include <string>

#include <Wt/Dbo/Session.h>

#include "database/profiling/IQueryProfiler.hpp"
#include "math/StatsAccumulator.hpp"

namespace lms::db
{
    class QueryProfiler : public IQueryProfiler
    {
    public:
        QueryProfiler();
        ~QueryProfiler() override;
        QueryProfiler(const QueryProfiler&) = delete;
        QueryProfiler& operator=(const QueryProfiler&) = delete;

        void visitQueries(const QueryVisitor& visitor) const override;

        void recordQueryExecution(Wt::Dbo::Session& session, const std::string& query, Clock::duration elapsed);

    private:
        void recordQueryPlan(Wt::Dbo::Session& session, const std::string& query);

        struct QueryData
        {
            std::string plan;
            math::StatsAccumulator<double> timeStats; // in Us
        };

        mutable std::shared_mutex _mutex;
        std::map<std::string, QueryData> _queries;
    };
} // namespace lms::db
