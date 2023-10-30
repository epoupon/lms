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

#include <Wt/WDateTime.h>
#include <Wt/WDate.h>
#include <Wt/WTime.h>
#include "utils/String.hpp"

TEST(StringUtils, splitString)
{
	{
		const std::string test{ "a" };

		const std::vector<std::string_view> strings{ StringUtils::splitString(test, "") };
		ASSERT_EQ(strings.size(), 1);
		EXPECT_EQ(strings.front(), "a");
	}

	{
		const std::string test{ "a b" };

		const std::vector<std::string_view> strings{ StringUtils::splitString(test, "|") };
		ASSERT_EQ(strings.size(), 1);
		EXPECT_EQ(strings.front(), "a b");
	}

	{
		const std::string test{ "  a" };

		const std::vector<std::string_view> strings{ StringUtils::splitString(test, " ") };
		ASSERT_EQ(strings.size(), 1);
		EXPECT_EQ(strings.front(), "a");
	}

	{
		const std::string test{ "a  " };

		const std::vector<std::string_view> strings{ StringUtils::splitString(test, " ") };
		ASSERT_EQ(strings.size(), 1);
		EXPECT_EQ(strings.front(), "a");
	}

	{
		const std::string test{ "a b" };

		const std::vector<std::string_view> strings{ StringUtils::splitString(test, " ") };
		ASSERT_EQ(strings.size(), 2);
		EXPECT_EQ(strings.front(), "a");
		EXPECT_EQ(strings.back(), "b");
	}

	{
		const std::string test{ "a b,c|defgh  " };

		const std::vector<std::string_view> strings{ StringUtils::splitString(test, " ,|") };
		ASSERT_EQ(strings.size(), 4);
		EXPECT_EQ(strings[0], "a");
		EXPECT_EQ(strings[1], "b");
		EXPECT_EQ(strings[2], "c");
		EXPECT_EQ(strings[3], "defgh");
	}
}


TEST(StringUtils, splitStringCopy)
{
	{
		const std::string test{ "test=foo" };

		const std::vector<std::string> strings{ StringUtils::splitStringCopy(test, "=") };
		ASSERT_EQ(strings.size(), 2);
		EXPECT_EQ(strings[0], "test");
		EXPECT_EQ(strings[1], "foo");
	}

	{
		const std::string test{ "test=foo bar" };

		const std::vector<std::string> strings{ StringUtils::splitStringCopy(test, "=") };
		ASSERT_EQ(strings.size(), 2);
		EXPECT_EQ(strings[0], "test");
		EXPECT_EQ(strings[1], "foo bar");
	}
}

TEST(StringUtils, escapeJSString)
{
	EXPECT_EQ(StringUtils::jsEscape(""), "");
	EXPECT_EQ(StringUtils::jsEscape(R"(Test'.mp3)"), R"(Test\'.mp3)");
	EXPECT_EQ(StringUtils::jsEscape(R"(Test"".mp3)"), R"(Test\"\".mp3)");
	EXPECT_EQ(StringUtils::jsEscape(R"(\Test\.mp3)"), R"(\\Test\\.mp3)");
}

TEST(StringUtils, escapeJsonString)
{
	EXPECT_EQ(StringUtils::jsonEscape(""), "");
	EXPECT_EQ(StringUtils::jsonEscape(R"(Test'.mp3)"), R"(Test'.mp3)");
	EXPECT_EQ(StringUtils::jsonEscape(R"(Test"".mp3)"), R"(Test\"\".mp3)");
	EXPECT_EQ(StringUtils::jsonEscape(R"(\Test\.mp3)"), R"(\\Test\\.mp3)");
}

TEST(StringUtils, escapeString)
{
	EXPECT_EQ(StringUtils::escapeString("", "*", ' '), "");
	EXPECT_EQ(StringUtils::escapeString("", "", ' '), "");
	EXPECT_EQ(StringUtils::escapeString("a", "", ' '), "a");
	EXPECT_EQ(StringUtils::escapeString("*", "*", '_'), "_*");
	EXPECT_EQ(StringUtils::escapeString("*a*", "*", '_'), "_*a_*");
	EXPECT_EQ(StringUtils::escapeString("*a|", "*|", '_'), "_*a_|");
	EXPECT_EQ(StringUtils::escapeString("**||", "*|", '_'), "_*_*_|_|");
}

TEST(StringUtils, readAs)
{
	EXPECT_EQ(StringUtils::readAs<bool>("true"), true);
	EXPECT_EQ(StringUtils::readAs<bool>("1"), true);
	EXPECT_EQ(StringUtils::readAs<bool>("false"), false);
	EXPECT_EQ(StringUtils::readAs<bool>("0"), false);
	EXPECT_EQ(StringUtils::readAs<bool>("foo"), std::nullopt);
	EXPECT_EQ(StringUtils::readAs<bool>(""), std::nullopt);
}

TEST(StringUtils, capitalize)
{
	struct TestCase
	{
		std::string input;
		std::string expectedOutput;
	};

	TestCase tests[]
	{
		{"", ""},
		{"C", "C"},
		{"c", "C"},
		{" c", " C"},
		{" cc", " Cc"},
		{"(c", "(c"},
		{"1c", "1c"},
		{"&c", "&c"},
		{"c c", "C c"}
	};

	for (const TestCase& test : tests)
	{
		std::string str{ test.input };
		StringUtils::capitalize(str);
		EXPECT_EQ(str, test.expectedOutput) << " str was '" << test.input << "'";
	}
}

TEST(Stringutils, date)
{
	const Wt::WDate date{ 2020, 01, 03 };
	EXPECT_EQ(StringUtils::toISO8601String(date), "2020-01-03");
}

TEST(Stringutils, dateTime)
{
	const Wt::WDateTime dateTime{ Wt::WDate {2020, 01, 03 }, Wt::WTime{9, 8, 11, 75} };
	EXPECT_EQ(StringUtils::toISO8601String(dateTime), "2020-01-03T09:08:11.075");
}