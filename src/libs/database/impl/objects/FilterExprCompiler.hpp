/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <string_view>

#include "database/objects/FilterExpr.hpp"
#include "database/objects/UserId.hpp"

#include "SqlFragment.hpp"

namespace lms::db
{
    // Returns the WHERE condition the tracks, named "t", must satisfy. Needs no join
    SqlFragment compileFilterExpr(const FilterExpr& expr, UserId user);

    // Returns "<column> IN (SELECT <trackColumn> FROM track t WHERE ...)": column is in the trackColumn ("t.id", "t.release_id"...) of a track matching the expression.
    // For the queries whose rows are not tracks. Uncorrelated: SQLite builds the set once instead of evaluating the expression for each row of the query
    SqlFragment createMatchingTracksCondition(std::string_view column, const FilterExpr& expr, UserId user, std::string_view trackColumn = "t.id");
} // namespace lms::db
