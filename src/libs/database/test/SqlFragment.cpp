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

#include <vector>

#include <gtest/gtest.h>

#include "SqlFragment.hpp"

namespace lms::db::tests
{
    TEST(SqlFragment, default_isEmpty)
    {
        const SqlFragment fragment;
        EXPECT_TRUE(fragment.isEmpty());
    }

    TEST(SqlFragment, withSql_isNotEmpty)
    {
        const SqlFragment fragment{ "a = 1" };
        EXPECT_FALSE(fragment.isEmpty());
    }

    TEST(SqlFragment, append_keepsValuesWithTheirPlaceholders)
    {
        SqlFragment fragment{ "a = ?", { 1 } };
        fragment.append(SqlFragment{ " AND b = ?", { std::string{ "b" } } });
        fragment.append(SqlFragment{});
        fragment.append(" AND c = ?", { 3LL });

        EXPECT_EQ(fragment.getSql(), "a = ? AND b = ? AND c = ?");
        EXPECT_EQ(fragment.getBinds(), (std::vector<BindValue>{ 1, std::string{ "b" }, 3LL }));
    }

    TEST(SqlFragment, append_valuesFromSpan)
    {
        const std::vector<BindValue> values{ 1, 2 };

        SqlFragment fragment{ "a BETWEEN ? AND ?", values };
        fragment.append(" AND b = ?", std::vector<BindValue>{ 3 });

        EXPECT_EQ(fragment.getBinds(), (std::vector<BindValue>{ 1, 2, 3 }));
    }

    TEST(SqlFragment, combineWithAnd_ignoresEmptySides)
    {
        const SqlFragment fragment{ "a = ?", { 1 } };

        SqlFragment lhsEmpty;
        lhsEmpty.combineWithAnd(fragment);
        EXPECT_EQ(lhsEmpty.getSql(), fragment.getSql());
        EXPECT_EQ(lhsEmpty.getBinds(), fragment.getBinds());

        SqlFragment rhsEmpty{ fragment };
        rhsEmpty.combineWithAnd(SqlFragment{});
        EXPECT_EQ(rhsEmpty.getSql(), fragment.getSql());
        EXPECT_EQ(rhsEmpty.getBinds(), fragment.getBinds());

        SqlFragment bothEmpty;
        bothEmpty.combineWithAnd(SqlFragment{});
        EXPECT_TRUE(bothEmpty.isEmpty());
    }

    TEST(SqlFragment, combineWithAnd_keepsBindsInTextualOrder)
    {
        SqlFragment combined{ "a = ? OR b = ?", { 1, std::string{ "b" } } };
        combined.combineWithAnd(SqlFragment{ "c = ?", { 3LL } });

        EXPECT_EQ(combined.getSql(), "(a = ? OR b = ?) AND (c = ?)");
        EXPECT_EQ(combined.getBinds(), (std::vector<BindValue>{ 1, std::string{ "b" }, 3LL }));
    }
} // namespace lms::db::tests
