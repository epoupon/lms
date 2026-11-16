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

#include <string>

#include <gtest/gtest.h>

#include "core/Ascii.hpp"

namespace lms::core::ascii::tests
{
    TEST(Ascii, nonAsciiChars)
    {
        // negative chars: UB with <cctype>
        EXPECT_FALSE(isAlpha('\xC3'));
        EXPECT_FALSE(isDigit('\xD9'));
        EXPECT_FALSE(isSpace('\xA0'));
        EXPECT_EQ(toUpper('\xA9'), '\xA9');
    }

    TEST(Ascii, toLowerUpper)
    {
        EXPECT_EQ(toLower("AbC 1-\xC3\x89"), "abc 1-\xC3\x89");
        EXPECT_EQ(toUpper("aBc 1-\xC3\xA9"), "ABC 1-\xC3\xA9");
    }

    TEST(Ascii, caseInsensitiveEqual)
    {
        EXPECT_TRUE(caseInsensitiveEqual("FLAC", "flac"));
        EXPECT_TRUE(caseInsensitiveEqual("", ""));
        EXPECT_FALSE(caseInsensitiveEqual("flac", "fla"));
        EXPECT_FALSE(caseInsensitiveEqual("\xC3\x89", "\xC3\xA9")); // É / é
    }

    TEST(Ascii, caseInsensitiveContains)
    {
        EXPECT_TRUE(caseInsensitiveContains("FooBar", "Bar"));
        EXPECT_TRUE(caseInsensitiveContains("FooBar", "bar"));
        EXPECT_TRUE(caseInsensitiveContains("FooBar", "Foo"));
        EXPECT_TRUE(caseInsensitiveContains("FooBar", "foo"));
        EXPECT_FALSE(caseInsensitiveContains("something", "foo"));
        EXPECT_TRUE(caseInsensitiveContains("FooBar", ""));
        EXPECT_TRUE(caseInsensitiveContains("", ""));
        EXPECT_FALSE(caseInsensitiveContains("", "Foo"));
        EXPECT_TRUE(caseInsensitiveContains("\xC3\x89t\xC3\xA9", "T\xC3\xA9"));
        EXPECT_FALSE(caseInsensitiveContains("\xC3\x89t\xC3\xA9", "\xC3\xA9t"));
    }

    TEST(Ascii, capitalize)
    {
        struct TestCase
        {
            std::string input;
            std::string expectedOutput;
        };

        TestCase tests[]{
            { "", "" },
            { "C", "C" },
            { "c", "C" },
            { " c", " C" },
            { " cc", " Cc" },
            { "(c", "(c" },
            { "1c", "1c" },
            { "&c", "&c" },
            { "c c", "C c" },
            { " \xC3\xA9t\xC3\xA9", " \xC3\xA9t\xC3\xA9" }
        };

        for (const TestCase& test : tests)
        {
            std::string str{ test.input };
            capitalize(str);
            EXPECT_EQ(str, test.expectedOutput) << " str was '" << test.input << "'";
        }
    }
} // namespace lms::core::ascii::tests
