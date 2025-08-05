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

#include "Utils.hpp"

#include "core/String.hpp"

namespace lms::db::utils
{
    std::string escapeForLikeKeyword(std::string_view keyword)
    {
        return core::stringUtils::escapeString(keyword, "%_", escapeChar);
    }

    Wt::WDateTime normalizeDateTime(const Wt::WDateTime& dateTime)
    {
        // force second resolution
        return Wt::WDateTime::fromTime_t(dateTime.toTime_t());
    }
} // namespace lms::db::utils
