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

#include <Wt/Dbo/StdSqlTraits.h>

#include "core/String.hpp"
#include "database/Types.hpp"

namespace Wt::Dbo
{
    template<>
    struct sql_value_traits<lms::db::ImageHashType>
    {
        static const bool specialized = true;
        // Uses an underlying string to encode this big value

        static std::string type(SqlConnection* conn, int size)
        {
            return sql_value_traits<std::string, void>::type(conn, size);
        }

        static void bind(const lms::db::ImageHashType& v, SqlStatement* statement, int column, int size)
        {
            std::string valueAsStr{ std::to_string(v.value()) };
            sql_value_traits<std::string>::bind(valueAsStr, statement, column, size);
        }

        static bool read(lms::db::ImageHashType& v, SqlStatement* statement, int column, int size)
        {
            std::string valueAsStr;
            if (sql_value_traits<std::string>::read(valueAsStr, statement, column, size))
            {
                if (const auto parsedValue{ lms::core::stringUtils::readAs<lms::db::ImageHashType::underlying_type>(valueAsStr) })
                {
                    v = lms::db::ImageHashType{ *parsedValue };
                    return true;
                }
            }

            v = lms::db::ImageHashType{};
            return false;
        }
    };
} // namespace Wt::Dbo
