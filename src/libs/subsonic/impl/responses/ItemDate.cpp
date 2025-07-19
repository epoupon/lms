/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "responses/Genre.hpp"

#include "database/objects/Cluster.hpp"

namespace lms::api::subsonic
{
    Response::Node createItemDateNode(const core::PartialDateTime& date)
    {
        Response::Node itemDateNode;

        if (auto year{ date.getYear() })
            itemDateNode.setAttribute("year", *year);
        if (auto month{ date.getMonth() })
            itemDateNode.setAttribute("month", *month);
        if (auto day{ date.getDay() })
            itemDateNode.setAttribute("day", *day);

        return itemDateNode;
    }
} // namespace lms::api::subsonic