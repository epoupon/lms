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

#include "core/PartialDateTime.hpp"

#include <Wt/Dbo/SqlTraits.h>

namespace Wt::Dbo
{
    template<>
    struct sql_value_traits<lms::core::PartialDateTime>
    {
        static std::string type(SqlConnection* conn, int /*size*/)
        {
            return conn->dateTimeType(SqlDateTimeType::DateTime);
        }

        static void bind(const lms::core::PartialDateTime& dateTime, SqlStatement* statement, int column, int /* size */)
        {
            if (!dateTime.isValid())
                statement->bindNull(column);
            else
                statement->bind(column, dateTime.toString());
        }

        static bool read(lms::core::PartialDateTime& dateTime, SqlStatement* statement, int column, int size)
        {
            std::string str;
            if (!statement->getResult(column, &str, size))
                return false;

            dateTime = lms::core::PartialDateTime::fromString(str);
            return true;
        }
    };
} // namespace Wt::Dbo
