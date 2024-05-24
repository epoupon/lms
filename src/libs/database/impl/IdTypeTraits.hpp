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

#pragma once

#include <type_traits>

#include <Wt/Dbo/StdSqlTraits.h>

#include "database/Types.hpp"

namespace Wt::Dbo
{
    template<typename T>
    struct sql_value_traits<T, typename std::enable_if<std::is_base_of<lms::db::IdType, T>::value>::type>
    {
        static_assert(!std::is_same_v<lms::db::IdType, T>, "Cannot use IdType, use derived types");
        static const bool specialized = true;

        static std::string type(SqlConnection* conn, int size)
        {
            return sql_value_traits<typename T::ValueType, void>::type(conn, size);
        }

        static void bind(const T& v, SqlStatement* statement, int column, int size)
        {
            sql_value_traits<typename T::ValueType>::bind(v.getValue(), statement, column, size);
        }

        static bool read(T& v, SqlStatement* statement, int column, int size)
        {
            typename T::ValueType value;
            if (sql_value_traits<typename T::ValueType>::read(value, statement, column, size))
            {
                v = value;
                return true;
            }

            v = {};
            return false;
        }
    };
} // namespace Wt::Dbo
