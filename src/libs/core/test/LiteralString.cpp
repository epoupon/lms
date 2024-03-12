/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <unordered_map>
#include <gtest/gtest.h>

#include "core/LiteralString.hpp"

namespace lms::core
{
    TEST(LiteralString, ctr)
    {
        {
            constexpr LiteralString foo{ "abc" };
            static_assert(foo.length() == 3);
            static_assert(foo == "abc");
            static_assert(foo == LiteralString{ "abc" });
            static_assert(foo < LiteralString{ "abcd" });
            static_assert(foo > LiteralString{ "aac" });

            EXPECT_EQ(::strlen(foo.c_str()), 3);
            EXPECT_EQ(foo, LiteralString{ "abc" });
            EXPECT_GT(foo, LiteralString{ "abb" });
            EXPECT_LT(foo, LiteralString{ "abcd" });
        }

        {
            constexpr LiteralString foo{ "" };
            static_assert(foo.length() == 0);
            static_assert(foo == "");
            static_assert(foo == LiteralString{ "" });
            static_assert(foo < LiteralString{ "a" });
            EXPECT_EQ(::strlen(foo.c_str()), 0);
        }
    }

    TEST(LiteralString, unordered_map)
    {
        {
            std::unordered_map<LiteralString, int> myMap{ {"abc", 42} };

            EXPECT_TRUE(myMap.contains("abc"));
            EXPECT_TRUE(myMap.contains(LiteralString{ "abc" }));
            EXPECT_FALSE(myMap.contains("abcd"));
        }

        {
            std::unordered_map<LiteralString, int, LiteralStringHash, LiteralStringEqual> myMap{ {"abc", 42} };
            EXPECT_TRUE(myMap.contains(std::string{ "abc" }));
            EXPECT_TRUE(myMap.contains(std::string_view{ "abc" }));
            EXPECT_FALSE(myMap.contains(std::string{ "abcd" }));
            EXPECT_FALSE(myMap.contains(std::string_view{ "abcd" }));
        }

    }
}