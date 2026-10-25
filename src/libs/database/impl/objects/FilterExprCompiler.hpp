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

#include <optional>

#include "database/objects/FilterExpr.hpp"
#include "database/objects/UserId.hpp"

#include "JoinSet.hpp"
#include "SqlFragment.hpp"

namespace lms::db
{
    // Both the entity a filter is evaluated for, and the entity a condition is about
    enum class FilterEntity
    {
        Track,
        Release,
        Artist,
    };

    // Whether the expression needs the rows of the tracks of the queried entity
    bool requiresTracks(const FilterExpr& expr, FilterEntity queriedEntity);

    // Returns the WHERE condition of the expression, empty if there is none, and adds to joins the joins this condition relies on:
    // the JoinSet must then be applied to the query before any WHERE bind.
    // The queries must name the queried entity "t", "r" or "a" (track, release, artist), and its tracks "t" if requiresTracks(),
    // joined before the call. Conditions on the queried entity apply to its own row. Track conditions apply to the track rows, all to a single track.
    // The aliases of the joins are numbered per call, and the ones of per-user conditions are fixed:
    // a JoinSet can only receive one compiled expression, or several of them if they use the same user
    SqlFragment compileFilterExpr(const std::optional<FilterExpr>& expr, FilterEntity queriedEntity, UserId user, JoinSet& joins);
} // namespace lms::db
