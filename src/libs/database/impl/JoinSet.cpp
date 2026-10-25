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

#include "JoinSet.hpp"

#include <algorithm>
#include <cassert>

namespace lms::db
{
    namespace
    {
        // "<table or subquery> <alias> ON <condition>": the last " ON " is the one of the join, as a subquery can contain its own
        std::string_view getAlias(std::string_view clause)
        {
            const std::size_t aliasEnd{ clause.rfind(" ON ") };
            assert(aliasEnd != std::string_view::npos && aliasEnd > 0);

            const std::size_t aliasBegin{ clause.rfind(' ', aliasEnd - 1) };
            assert(aliasBegin != std::string_view::npos);

            return clause.substr(aliasBegin + 1, aliasEnd - aliasBegin - 1);
        }
    } // namespace

    void JoinSet::add(std::string_view clause, Type type, std::initializer_list<BindValue> binds)
    {
        add(SqlFragment{ clause, binds }, type);
    }

    void JoinSet::add(const SqlFragment& join, Type type)
    {
        const std::string_view alias{ getAlias(join.getSql()) };
        const auto it{ std::find_if(std::cbegin(_joins), std::cend(_joins), [&](const Join& existing) { return getAlias(existing.on.getSql()) == alias; }) };
        if (it != std::cend(_joins))
        {
            // An alias must always designate the same join: a different one means a clash, such as several filters or users in the same set
            assert(it->type == type && it->on.getSql() == join.getSql() && it->on.getBinds() == join.getBinds());
            return;
        }

        _joins.push_back(Join{ type, join });
    }

    bool JoinSet::hasAlias(std::string_view alias) const
    {
        return std::any_of(std::cbegin(_joins), std::cend(_joins), [&](const Join& join) { return getAlias(join.on.getSql()) == alias; });
    }

    SqlFragment JoinSet::toSql() const
    {
        SqlFragment res;
        for (const Join& join : _joins)
        {
            res.append(join.type == Type::Left ? " LEFT JOIN " : " JOIN ");
            res.append(join.on);
        }

        return res;
    }
} // namespace lms::db
