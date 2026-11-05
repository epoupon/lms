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

#include "SqlFragment.hpp"

#include <algorithm>
#include <cassert>

namespace lms::db
{
    SqlFragment::SqlFragment(std::string_view sql, std::initializer_list<BindValue> binds)
        : SqlFragment{ sql, std::span<const BindValue>{ binds.begin(), binds.size() } }
    {
    }

    SqlFragment::SqlFragment(std::string_view sql, std::span<const BindValue> binds)
        : _sql{ sql }
        , _binds{ std::cbegin(binds), std::cend(binds) }
    {
#ifndef NDEBUG
        checkWellFormed();
#endif // NDEBUG
    }

    bool SqlFragment::isEmpty() const
    {
        return _sql.empty();
    }

    void SqlFragment::append(std::string_view sql, std::initializer_list<BindValue> binds)
    {
        append(sql, std::span<const BindValue>{ binds.begin(), binds.size() });
    }

    void SqlFragment::append(std::string_view sql, std::span<const BindValue> binds)
    {
        _sql += sql;
        _binds.insert(std::end(_binds), std::cbegin(binds), std::cend(binds));
#ifndef NDEBUG
        checkWellFormed();
#endif // NDEBUG
    }

    void SqlFragment::append(const SqlFragment& other)
    {
        append(other._sql, other._binds);
    }

    void SqlFragment::combineWithAnd(const SqlFragment& other)
    {
        if (other.isEmpty())
            return;

        if (isEmpty())
        {
            *this = other;
            return;
        }

        _sql.insert(0, "(");
        append(") AND (");
        append(other);
        append(")");
    }

#ifndef NDEBUG
    void SqlFragment::checkWellFormed() const
    {
        assert(static_cast<std::size_t>(std::count(std::cbegin(_sql), std::cend(_sql), '?')) == _binds.size());
    }
#endif // NDEBUG
} // namespace lms::db
