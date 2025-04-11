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

#include <gtest/gtest.h>

#include <Wt/WDate.h>
#include <Wt/WDateTime.h>
#include <Wt/WTime.h>

#include "core/String.hpp"

namespace lms::core::stringUtils::tests
{
    TEST(StringUtils, splitString_charDelim)
    {
        struct TestCase
        {
            std::string_view input;
            char delimiter;
            std::vector<std::string_view> expectedOutput;
        };

        TestCase tests[]{
            { "abc", '-', { "abc" } },
            { "a", '-', { "a" } },
            { "", '-', { "" } },
            { "a-b-c", '-', { "a", "b", "c" } },
            { "a|b|c", '|', { "a", "b", "c" } },
            { "a;b;c", ';', { "a", "b", "c" } },
            { ";b;c", ';', { "", "b", "c" } },
            { " ;b;c", ';', { " ", "b", "c" } },
            { " ;;c", ';', { " ", "", "c" } },
            { " ; ;c", ';', { " ", " ", "c" } },
            { "a;b; ", ';', { "a", "b", " " } },
            { "a;b", ';', { "a", "b" } },
            { ";b", ';', { "", "b" } },
            { ";", ';', { "", "" } },
            { ";;", ';', { "", "", "" } },
            { ";;;", ';', { "", "", "", "" } },
            { ";;a;;b;;", ';', { "", "", "a", "", "b", "", "" } },
            { "a b", ' ', { "a", "b" } },
            { "", ' ', { "" } },
            { "a-b|c", '-', { "a", "b|c" } },
            { "a|b-c", '-', { "a|b", "c" } },
            { "test=foo bar", '=', { "test", "foo bar" } },
        };

        for (const TestCase& test : tests)
        {
            const std::vector<std::string_view> res{ splitString(test.input, test.delimiter) };
            EXPECT_EQ(res, test.expectedOutput) << "Input = '" << test.input << "', delims = '" << test.delimiter << "'";
        }
    }

    TEST(StringUtils, splitString_stringDelim)
    {
        struct TestCase
        {
            std::string_view input;
            std::string_view delimiter;
            std::vector<std::string_view> expectedOutput;
        };

        TestCase tests[]{
            { "", "", { "" } },
            { "//", "//", { "", "" } },
            { "//abc//", "//", { "", "abc", "" } },
            { "//abc////abc//", "//", { "", "abc", "", "abc", "" } },
            { "abc", "", { "abc" } },
            { "abc", "-", { "abc" } },
            { "abc", "b", { "a", "c" } },
            { "ab/cd", "/", { "ab", "cd" } },
            { "ab/cd", "/ ", { "ab/cd" } },
            { "ab/cd", " /", { "ab/cd" } },
            { "ab /cd", " /", { "ab", "cd" } },
            { "ab/ cd", "/ ", { "ab", "cd" } },
            { "ab / cd", " / ", { "ab", "cd" } },
            { "ab/cd", " / ", { "ab/cd" } },
            { "ab/cd / ", " / ", { "ab/cd", "" } },
        };

        for (const TestCase& test : tests)
        {
            const std::vector<std::string_view> res{ splitString(test.input, test.delimiter) };
            EXPECT_EQ(res, test.expectedOutput) << "Input = '" << test.input << "', delims = '" << test.delimiter << "'";
        }
    }

    TEST(StringUtils, splitString_multiStringDelim)
    {
        struct TestCase
        {
            std::string_view input;
            std::vector<std::string_view> delimiters;
            std::vector<std::string_view> expectedOutput;
        };

        TestCase tests[]{
            { "", { "" }, { "" } },
            { "abc", { "" }, { "abc" } },
            { "abc", { "b" }, { "a", "c" } },
            { "ab/cd", { "/" }, { "ab", "cd" } },
            { "ab/cd", { "/", ";" }, { "ab", "cd" } },
            { "ab;/cd", { "/", ";" }, { "ab", "", "cd" } },
            { "ab;/;cd", { "/", ";" }, { "ab", "", "", "cd" } },
            { "ab/;cd", { "/", ";" }, { "ab", "", "cd" } },
            { "ab/;/cd", { "/", ";" }, { "ab", "", "", "cd" } },
            { "ab/cd/ef", { "/", "cd" }, { "ab", "", "", "ef" } },
        };

        for (const TestCase& test : tests)
        {
            const std::vector<std::string_view> res{ splitString(test.input, test.delimiters) };
            EXPECT_EQ(res, test.expectedOutput) << "Input = '" << test.input << "'";
        }
    }

    TEST(StringUtils, joinStrings)
    {
        struct TestCase
        {
            std::vector<std::string_view> input;
            std::string delimiter;
            std::string expectedOutput;
        };

        TestCase tests[]{
            { { "a", "b", "c" }, "-", "a-b-c" },
            { { "a", "b", "c" }, ",", "a,b,c" },
            { { "a", "b", "c" }, "***", "a***b***c" },
            { { "a", "", "c" }, "-", "a--c" },
            { { "", "b", "c" }, "-", "-b-c" },
            { { "a" }, "-", "a" },
            { { "a" }, ",", "a" },
        };

        for (const TestCase& test : tests)
        {
            const std::string str{ joinStrings(test.input, test.delimiter) };
            EXPECT_EQ(str, test.expectedOutput);
        }
    }

    TEST(StringUtils, escapeAndJoinStrings)
    {
        struct TestCase
        {
            std::vector<std::string_view> input;
            char delimiter;
            char escapeChar;
            std::string expectedOutput;
        };

        const TestCase tests[]{
            { { "" }, ';', '\\', "" },
            { { ";" }, ';', '\\', "\\;" },
            { { ";;" }, ';', '\\', "\\;\\;" },
            { { "a;", "b" }, ';', '\\', "a\\;;b" },
            { { "a;", "b;" }, ';', '\\', "a\\;;b\\;" },
        };

        for (const TestCase& test : tests)
        {
            const std::string str{ escapeAndJoinStrings(test.input, test.delimiter, test.escapeChar) };
            EXPECT_EQ(str, test.expectedOutput);
        }
    }

    TEST(StringUtils, splitEscapedStrings)
    {
        struct TestCase
        {
            std::string input;
            char delimiter;
            char escapeChar;
            std::vector<std::string> expectedOutput;
        };

        TestCase tests[]{
            { "", ';', '\\', {} },
            { "\\;", ';', '\\', { ";" } },
            { "\\;\\;", ';', '\\', { ";;" } },
            { "a\\;;b", ';', '\\', { "a;", "b" } },
            { "a\\;;b\\;", ';', '\\', { "a;", "b;" } },
        };

        for (const TestCase& test : tests)
        {
            const std::vector<std::string> str{ splitEscapedStrings(test.input, test.delimiter, test.escapeChar) };
            EXPECT_EQ(str, test.expectedOutput);
        }
    }

    TEST(StringUtils, escapeJSString)
    {
        EXPECT_EQ(jsEscape(""), "");
        EXPECT_EQ(jsEscape(R"(Test'.mp3)"), R"(Test\'.mp3)");
        EXPECT_EQ(jsEscape(R"(Test"".mp3)"), R"(Test\"\".mp3)");
        EXPECT_EQ(jsEscape(R"(\Test\.mp3)"), R"(\\Test\\.mp3)");
    }

    TEST(StringUtils, escapeJsonString)
    {
        EXPECT_EQ(jsonEscape(""), "");
        EXPECT_EQ(jsonEscape(R"(Test'.mp3)"), R"(Test'.mp3)");
        EXPECT_EQ(jsonEscape(R"(Test"".mp3)"), R"(Test\"\".mp3)");
        EXPECT_EQ(jsonEscape(R"(\Test\.mp3)"), R"(\\Test\\.mp3)");
    }

    TEST(StringUtils, escapeString)
    {
        EXPECT_EQ(escapeString("", "*", ' '), "");
        EXPECT_EQ(escapeString("", "", ' '), "");
        EXPECT_EQ(escapeString("a", "", ' '), "a");
        EXPECT_EQ(escapeString("*", "*", '_'), "_*");
        EXPECT_EQ(escapeString("*a*", "*", '_'), "_*a_*");
        EXPECT_EQ(escapeString("*a|", "*|", '_'), "_*a_|");
        EXPECT_EQ(escapeString("**||", "*|", '_'), "_*_*_|_|");
        EXPECT_EQ(escapeString("one;two", ";", '\\'), "one\\;two");
        EXPECT_EQ(escapeString("one\\;two", ";", '\\'), "one\\\\;two");
        EXPECT_EQ(escapeString("one;", ";", '\\'), "one\\;");
    }

    TEST(StringUtils, unescapeString)
    {
        EXPECT_EQ(unescapeString("one\\", '\\'), "one\\");
        EXPECT_EQ(unescapeString("\\\\one", '\\'), "\\one");
        EXPECT_EQ(unescapeString("one\\;two", '\\'), "one;two");
        EXPECT_EQ(unescapeString("one\\\\;two", '\\'), "one\\;two");
    }

    TEST(StringUtils, readAs_bool)
    {
        EXPECT_EQ(readAs<bool>("true"), true);
        EXPECT_EQ(readAs<bool>("1"), true);
        EXPECT_EQ(readAs<bool>("false"), false);
        EXPECT_EQ(readAs<bool>("0"), false);
        EXPECT_EQ(readAs<bool>("foo"), std::nullopt);
        EXPECT_EQ(readAs<bool>(""), std::nullopt);
    }

    TEST(StringUtils, readAs_int)
    {
        EXPECT_EQ(readAs<int>("1024"), 1024);
        EXPECT_EQ(readAs<int>("0"), 0);
        EXPECT_EQ(readAs<int>("-0"), 0);
        EXPECT_EQ(readAs<int>("-1"), -1);
        EXPECT_EQ(readAs<int>(""), std::nullopt);
        EXPECT_EQ(readAs<int>("a"), std::nullopt);
        EXPECT_EQ(readAs<int>("-"), std::nullopt);
        EXPECT_EQ(readAs<int>("1024-1"), 1024);
        EXPECT_EQ(readAs<int>("1024-"), 1024);
        EXPECT_EQ(readAs<int>("1024/5"), 1024);
        EXPECT_EQ(readAs<int>("1024a"), 1024);
        EXPECT_EQ(readAs<int>("a1024a"), std::nullopt);
    }

    TEST(StringUtils, capitalize)
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
            { "c c", "C c" }
        };

        for (const TestCase& test : tests)
        {
            std::string str{ test.input };
            capitalize(str);
            EXPECT_EQ(str, test.expectedOutput) << " str was '" << test.input << "'";
        }
    }

    TEST(Stringutils, DateToString)
    {
        {
            const Wt::WDate date{ 2020, 01, 03 };
            EXPECT_EQ(toISO8601String(date), "2020-01-03");
        }

        {
            const Wt::WDate date;
            EXPECT_EQ(toISO8601String(date), "");
        }
    }

    TEST(Stringutils, DateTimeToString)
    {
        {
            const Wt::WDateTime dateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 9, 8, 11, 75 } };
            EXPECT_EQ(toISO8601String(dateTime), "2020-01-03T09:08:11.075Z");
        }

        {
            const Wt::WDateTime dateTime{ Wt::WDate{ 2020, 01, 03 } };
            EXPECT_EQ(toISO8601String(dateTime), "2020-01-03T00:00:00.000Z");
        }

        {
            const Wt::WDateTime dateTime;
            EXPECT_EQ(toISO8601String(dateTime), "");
        }
    }

    TEST(Stringutils, DateTimeFromString)
    {
        EXPECT_EQ(fromISO8601String("2020-01-03T09:08:11.075Z"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 9, 8, 11, 75 } }));
        EXPECT_EQ(fromISO8601String("2020-01-03T09:08:11.075"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 9, 8, 11, 75 } }));
        EXPECT_EQ(fromISO8601String("2020-01-03"), Wt::WDateTime{});
        EXPECT_EQ(fromISO8601String(""), Wt::WDateTime{});
    }

    TEST(StringUtils, stringEndsWith)
    {
        EXPECT_TRUE(stringEndsWith("FooBar", "Bar"));
        EXPECT_TRUE(stringEndsWith("FooBar", ""));
        EXPECT_TRUE(stringEndsWith("", ""));
        EXPECT_TRUE(stringEndsWith("FooBar", "ar"));
        EXPECT_TRUE(stringEndsWith("FooBar", "FooBar"));
        EXPECT_FALSE(stringEndsWith("FooBar", "1FooBar"));
        EXPECT_FALSE(stringEndsWith("FooBar", "1FooBar"));
        EXPECT_FALSE(stringEndsWith("FooBar", "R"));
    }

    TEST(StringUtils, stringCaseInsensitiveContains)
    {
        EXPECT_TRUE(stringCaseInsensitiveContains("FooBar", "Bar"));
        EXPECT_TRUE(stringCaseInsensitiveContains("FooBar", "bar"));
        EXPECT_TRUE(stringCaseInsensitiveContains("FooBar", "Foo"));
        EXPECT_TRUE(stringCaseInsensitiveContains("FooBar", "foo"));
        EXPECT_FALSE(stringCaseInsensitiveContains("something", "foo"));
        EXPECT_TRUE(stringCaseInsensitiveContains("FooBar", ""));
        EXPECT_TRUE(stringCaseInsensitiveContains("", ""));
        EXPECT_FALSE(stringCaseInsensitiveContains("", "Foo"));
    }
} // namespace lms::core::stringUtils::tests