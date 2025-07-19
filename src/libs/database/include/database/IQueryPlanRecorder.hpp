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

#include <functional>
#include <memory>

namespace lms::db
{
    // Due to technical limitations, query plans are recorded globally across all databases.
    // As a result, this class is implemented as a singleton rather than being owned per DB instance.
    class IQueryPlanRecorder
    {
    public:
        virtual ~IQueryPlanRecorder() = default;

        using QueryPlanVisitor = std::function<void(std::string_view query, std::string_view plan)>;
        virtual void visitQueryPlans(const QueryPlanVisitor& visitor) const = 0;
    };

    std::unique_ptr<IQueryPlanRecorder> createQueryPlanRecorder();
} // namespace lms::db
