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
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <Wt/Dbo/Query.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/UUID.hpp"

#include "traits/UUIDTraits.hpp"

namespace lms::db
{
    using BindValue = std::variant<long long, int, bool, std::string, Wt::WDateTime, core::UUID>;

    // A piece of SQL and the values of its placeholders, in order.
    // The SQL and its values are always added together, and the SQL must not contain any '?' that is not a placeholder
    class SqlFragment
    {
    public:
        SqlFragment() = default;
        SqlFragment(std::string_view sql, std::initializer_list<BindValue> binds = {});
        SqlFragment(std::string_view sql, std::span<const BindValue> binds);

        bool isEmpty() const;
        const std::string& getSql() const { return _sql; }
        const std::vector<BindValue>& getBinds() const { return _binds; }

        void append(std::string_view sql, std::initializer_list<BindValue> binds = {});
        void append(std::string_view sql, std::span<const BindValue> binds);
        void append(const SqlFragment& other);

        // An empty side is ignored
        void combineWithAnd(const SqlFragment& other);

    private:
#ifndef NDEBUG
        void checkWellFormed() const;
#endif // NDEBUG

        std::string _sql;
        std::vector<BindValue> _binds;
    };

    template<typename ResultType>
    void bindFragment(Wt::Dbo::Query<ResultType>& query, const SqlFragment& fragment)
    {
        for (const BindValue& value : fragment.getBinds())
            std::visit([&query](const auto& boundValue) { query.bind(boundValue); }, value);
    }

    // Adds the fragment as a WHERE condition of the query, unless it is empty
    template<typename ResultType>
    void applyWhereFragment(Wt::Dbo::Query<ResultType>& query, const SqlFragment& fragment)
    {
        if (fragment.isEmpty())
            return;

        query.where(fragment.getSql());
        bindFragment(query, fragment);
    }
} // namespace lms::db
