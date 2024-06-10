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

#include <filesystem>
#include <string>

#include <Wt/Dbo/SqlTraits.h>

namespace Wt::Dbo
{
    template<>
    struct sql_value_traits<std::filesystem::path>
    {
        static std::string type(SqlConnection* conn, int size)
        {
            return sql_value_traits<std::string>::type(conn, size);
        }

        static void bind(const std::filesystem::path& path, SqlStatement* statement, int column, int /* size */)
        {
            statement->bind(column, path.string());
        }

        static bool read(std::filesystem::path& p, SqlStatement* statement, int column, int size)
        {
            std::string s;
            bool result = statement->getResult(column, &s, size);
            if (!result)
                return false;

            p = std::filesystem::path{ s };

            return true;
        }
    };
} // namespace Wt::Dbo
