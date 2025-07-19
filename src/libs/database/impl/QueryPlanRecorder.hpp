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

#include "database/IQueryPlanRecorder.hpp"

namespace lms::db
{
    class QueryPlanRecorder : public IQueryPlanRecorder
    {
    public:
        QueryPlanRecorder();
        ~QueryPlanRecorder() override;
        QueryPlanRecorder(const QueryPlanRecorder&) = delete;
        QueryPlanRecorder& operator=(const QueryPlanRecorder&) = delete;

        void visitQueryPlans(const QueryPlanVisitor& visitor) const override;

        void recordQueryPlanIfNeeded(Wt::Dbo::Session& session, const std::string& query);

    private:
        mutable std::shared_mutex _mutex;
        std::map<std::string, std::string> _queryPlans;
    };
} // namespace lms::db
