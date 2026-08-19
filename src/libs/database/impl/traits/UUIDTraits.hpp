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

#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

#include <Wt/Dbo/StdSqlTraits.h>

#include "core/UUID.hpp"

namespace Wt::Dbo
{
    template<>
    struct sql_value_traits<lms::core::UUID>
    {
        static constexpr bool specialized{ true };
        using UnderlyingType = std::vector<unsigned char>;

        static std::string type(SqlConnection* conn, int size)
        {
            return sql_value_traits<UnderlyingType, void>::type(conn, size);
        }

        static void bind(const lms::core::UUID& v, SqlStatement* statement, int column, int size)
        {
            constexpr auto binarySize{ lms::core::UUID::binarySize };
            const auto bytes{ v.bytes() };
            UnderlyingType blob(binarySize);
            std::memcpy(blob.data(), bytes.data(), binarySize);
            sql_value_traits<UnderlyingType>::bind(blob, statement, column, size);
        }

        static bool read(lms::core::UUID& v, SqlStatement* statement, int column, int size)
        {
            constexpr auto binarySize{ lms::core::UUID::binarySize };
            UnderlyingType buf;
            if (!sql_value_traits<UnderlyingType>::read(buf, statement, column, size) || buf.size() != binarySize)
                return false;

            v = lms::core::UUID::fromBytes(std::span<const std::byte, binarySize>{ reinterpret_cast<const std::byte*>(buf.data()), binarySize });
            return true;
        }
    };
} // namespace Wt::Dbo
