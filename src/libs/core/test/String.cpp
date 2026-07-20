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

#include <array>
#include <limits>

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
        EXPECT_EQ(jsonEscape("Line1\nLine2"), R"(Line1\nLine2)");
        EXPECT_EQ(jsonEscape("Line1\rLine2"), R"(Line1\rLine2)");
        EXPECT_EQ(jsonEscape("Col1\tCol2"), R"(Col1\tCol2)");
        EXPECT_EQ(jsonEscape("Hello\bWorld"), R"(Hello\bWorld)");
        EXPECT_EQ(jsonEscape("Hello\fWorld"), R"(Hello\fWorld)");
        EXPECT_EQ(jsonEscape("Hello\nWorld"), R"(Hello\nWorld)");
    }

    TEST(StringUtils, escapeXmlString)
    {
        EXPECT_EQ(xmlEscape(""), "");
        EXPECT_EQ(xmlEscape("Test.mp3"), "Test.mp3");
        EXPECT_EQ(xmlEscape("A & B"), "A &amp; B");
        EXPECT_EQ(xmlEscape("<tag>"), "&lt;tag&gt;");
        EXPECT_EQ(xmlEscape(R"(He said "Hello")"), "He said &quot;Hello&quot;");
        EXPECT_EQ(xmlEscape("It's fine"), "It&apos;s fine");
        EXPECT_EQ(xmlEscape(R"(<tag attr="val & val2">O'Hara</tag>)"), "&lt;tag attr=&quot;val &amp; val2&quot;&gt;O&apos;Hara&lt;/tag&gt;");
        EXPECT_EQ(xmlEscape(R"(\Test\.mp3)"), R"(\Test\.mp3)");
        EXPECT_EQ(xmlEscape("Café & Tea"), "Café &amp; Tea");
        EXPECT_EQ(xmlEscape(R"(&<>'")"), "&amp;&lt;&gt;&apos;&quot;");
        EXPECT_EQ(xmlEscape("Line1\nLine2"), "Line1\nLine2");
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

    TEST(Stringutils, DateTimeFromRFC822String)
    {
        EXPECT_EQ(fromRFC822String(""), Wt::WDateTime{});
        EXPECT_EQ(fromRFC822String("Mon, 3 Jan 2020 09:08:11 UT"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 9, 8, 11, 0 } }));
        EXPECT_EQ(fromRFC822String("3 Jan 2020 09:08:11 UT"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 9, 8, 11, 0 } }));
        EXPECT_EQ(fromRFC822String("3 Jan 2020 09:08:11 +0200"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 11, 8, 11, 0 } }));
        EXPECT_EQ(fromRFC822String("3 Jan 2020 09:08 +0200"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 11, 8, 00, 0 } }));
        EXPECT_EQ(fromRFC822String("3 Jan 2020 09:08:11 -0200"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 7, 8, 11, 0 } }));
        EXPECT_EQ(fromRFC822String("3 Jan 2020 01:08:11 -0200"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 02 }, Wt::WTime{ 23, 8, 11, 0 } }));
        EXPECT_EQ(fromRFC822String("3 Jan 2020 01:08:11 -0230"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 02 }, Wt::WTime{ 22, 38, 11, 0 } }));
        EXPECT_EQ(fromRFC822String("3 Jan 2020 10:08:11 CST"), (Wt::WDateTime{ Wt::WDate{ 2020, 01, 03 }, Wt::WTime{ 04, 8, 11, 0 } }));
        EXPECT_EQ(fromRFC822String("Sat, 09 Aug 2025 21:34:32 +0200"), (Wt::WDateTime{ Wt::WDate{ 2025, 8, 9 }, Wt::WTime{ 23, 34, 32 } }));
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

    TEST(StringUtils, utf8Truncate)
    {
        EXPECT_EQ(utf8Truncate("abc", 10), "abc");
        EXPECT_EQ(utf8Truncate("abcdef", 6), "abcdef");
        EXPECT_EQ(utf8Truncate("", 10), "");
        EXPECT_EQ(utf8Truncate("abc", 0), "");

        EXPECT_EQ(utf8Truncate("caf\xC3\xA9", 4), "caf");
        EXPECT_EQ(utf8Truncate("caf\xC3\xA9", 5), "caf\xC3\xA9");

        EXPECT_EQ(utf8Truncate("\xE2\x82\xAC", 1), "");
        EXPECT_EQ(utf8Truncate("\xE2\x82\xAC", 2), "");
        EXPECT_EQ(utf8Truncate("\xE2\x82\xAC", 3), "\xE2\x82\xAC");

        EXPECT_EQ(utf8Truncate("\xF0\x9F\x98\x80", 1), "");
        EXPECT_EQ(utf8Truncate("\xF0\x9F\x98\x80", 2), "");
        EXPECT_EQ(utf8Truncate("\xF0\x9F\x98\x80", 3), "");
        EXPECT_EQ(utf8Truncate("\xF0\x9F\x98\x80", 4), "\xF0\x9F\x98\x80");

        EXPECT_EQ(utf8Truncate("e\xCC\x81", 2), "e");
        EXPECT_EQ(utf8Truncate("e\xCC\x81", 3), "e\xCC\x81");

        EXPECT_EQ(utf8Truncate("\x80\x80\x80", 2), "");
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

    TEST(StringUtils, bufferToHexString)
    {
        EXPECT_EQ(bufferToHexString({}), "");
        EXPECT_EQ(bufferToHexString(std::array{ std::byte{ 0x31 }, std::byte{ 0x32 }, std::byte{ 0x33 } }), "313233");
        EXPECT_EQ(bufferToHexString(std::array{ std::byte{ 0x00 }, std::byte{ 0xab }, std::byte{ 0xcd }, std::byte{ 0xff } }), "00ABCDFF");

        // test back stringFromHex
        EXPECT_EQ(stringFromHex(""), "");
        EXPECT_EQ(stringFromHex("313233"), "123");
        EXPECT_EQ(stringFromHex("31323334"), "1234");
        EXPECT_EQ(stringFromHex("3132333435"), "12345");
        EXPECT_EQ(stringFromHex("54657374"), "Test");
    }

    TEST(StringUtils, toRomanNumeral)
    {
        EXPECT_EQ(toRomanNumeral(1), "i");
        EXPECT_EQ(toRomanNumeral(2), "ii");
        EXPECT_EQ(toRomanNumeral(3), "iii");
        EXPECT_EQ(toRomanNumeral(4), "iv");
        EXPECT_EQ(toRomanNumeral(5), "v");
        EXPECT_EQ(toRomanNumeral(6), "vi");
        EXPECT_EQ(toRomanNumeral(7), "vii");
        EXPECT_EQ(toRomanNumeral(8), "viii");
        EXPECT_EQ(toRomanNumeral(9), "ix");
        EXPECT_EQ(toRomanNumeral(10), "x");
        EXPECT_EQ(toRomanNumeral(11), "xi");
        EXPECT_EQ(toRomanNumeral(14), "xiv");
        EXPECT_EQ(toRomanNumeral(16), "xvi");
        EXPECT_EQ(toRomanNumeral(40), "xl");
        EXPECT_EQ(toRomanNumeral(50), "l");
        EXPECT_EQ(toRomanNumeral(90), "xc");
        EXPECT_EQ(toRomanNumeral(99), "xcix");
        EXPECT_EQ(toRomanNumeral(444), "cdxliv");
        EXPECT_EQ(toRomanNumeral(1994), "mcmxciv");
        EXPECT_EQ(toRomanNumeral(3999), "mmmcmxcix");
        EXPECT_EQ(toRomanNumeral(0), "");
        EXPECT_EQ(toRomanNumeral(4000), "");
        EXPECT_EQ(toRomanNumeral(static_cast<std::size_t>(-1)), ""); // underflows to SIZE_MAX
        EXPECT_EQ(toRomanNumeral(std::numeric_limits<std::size_t>::max()), "");
    }
} // namespace lms::core::stringUtils::tests