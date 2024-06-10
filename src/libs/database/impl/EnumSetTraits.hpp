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

#pragma once

#include <type_traits>

#include <Wt/Dbo/StdSqlTraits.h>

#include "core/EnumSet.hpp"

namespace Wt::Dbo
{
    template<typename T>
    struct sql_value_traits<lms::core::EnumSet<T>, void> : public sql_value_traits<long long>
    {
        using ValueType = typename lms::core::EnumSet<T>::ValueType;
        static_assert(sizeof(long long) > sizeof(ValueType));

        static void bind(lms::core::EnumSet<T> v, SqlStatement* statement, int column, int size)
        {
            sql_value_traits<long long>::bind(static_cast<long long>(v.getBitfield()), statement, column, size);
        }

        static bool read(lms::core::EnumSet<T>& v, SqlStatement* statement, int column, int size)
        {
            long long val;
            if (sql_value_traits<long long>::read(val, statement, column, size))
            {
                v.setBitfield(val);
                return true;
            }
            v.clear();
            return false;
        }
    };
} // namespace Wt::Dbo
