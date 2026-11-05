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

#include <initializer_list>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Query.h>

#include "SqlFragment.hpp"

namespace lms::db
{
    // Joins needed by the different parts of a query (sort, parameters), each emitted once.
    // Must be applied before any WHERE bind, since joins may carry placeholders
    class JoinSet
    {
    public:
        enum class Type
        {
            Inner,
            Left,
        };

        // The join is "<table or subquery> <alias> ON <condition>" and the values of its placeholders.
        // Joins are emitted in insertion order. An alias designates a single join: adding it again is a no-op if the join is the same, otherwise it is an error
        void add(std::string_view clause, Type type = Type::Inner, std::initializer_list<BindValue> binds = {});
        void add(const SqlFragment& join, Type type = Type::Inner);

        bool empty() const { return _joins.empty(); }

        // Renders the joins as SQL, for a query that cannot take them through apply (such as a subquery)
        SqlFragment toSql() const;

        template<typename ResultType>
        void apply(Wt::Dbo::Query<ResultType>& query) const
        {
            for (const Join& join : _joins)
            {
                if (join.type == Type::Left)
                    query.leftJoin(join.on.getSql());
                else
                    query.join(join.on.getSql());

                bindFragment(query, join.on);
            }
        }

    private:
        struct Join
        {
            Type type;
            SqlFragment on; // the clause and the values of its placeholders
        };

        std::vector<Join> _joins;
    };
} // namespace lms::db
