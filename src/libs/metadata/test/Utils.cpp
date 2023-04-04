
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

#include "Utils.hpp"

TEST(MetaData, parseDate)
{
	using namespace MetaData::Utils;

	struct TestCase
	{
		std::string	str;
		Wt::WDate	result;
	} testCases []
	{
		{ "1995-05-09",	Wt::WDate {1995, 5, 9} },
		{ "1995-01-01",	Wt::WDate {1995, 1, 1} },
		{ "1900-01-01",	Wt::WDate {1900, 1, 1} },
		{ "1899-01-01",	Wt::WDate {1899, 1, 1} },
		{ "1899-12-31",	Wt::WDate {1899, 12, 31} },
		{ "1899-11-30",	Wt::WDate {1899, 11, 30} },
		{ "1500-11-30",	Wt::WDate {1500, 11, 30} },
		{ "1000-11-30",	Wt::WDate {1000, 11, 30} },
		{ "1899-11-31",	Wt::WDate {} },					// invalid day
		{ "1899-13-01",	Wt::WDate {} },			 		// invalid month
		{ "1899-11",	Wt::WDate {1899, 11, 1} },		// missing day
		{ "1899",		Wt::WDate {1899, 1, 1} },		// missing month and days
		{ "1600",		Wt::WDate {1600, 1, 1} },		// missing month and days
		{ "1995/05/09",	Wt::WDate {1995, 5, 9} },
		{ "1995/01/01",	Wt::WDate {1995, 1, 1} },
		{ "1900/01/01",	Wt::WDate {1900, 1, 1} },
		{ "1899/01/01",	Wt::WDate {1899, 1, 1} },
		{ "1899/12/31",	Wt::WDate {1899, 12, 31} },
		{ "1899/11/30",	Wt::WDate {1899, 11, 30} },
		{ "1500/11/30",	Wt::WDate {1500, 11, 30} },
		{ "1000/11/30",	Wt::WDate {1000, 11, 30} },
		{ "1899/11/31",	Wt::WDate {} },					// invalid day
		{ "1899/13/01",	Wt::WDate {} },			 		// invalid month
		{ "1899/11",	Wt::WDate {1899, 11, 1} },		// missing day
		{ "1899",		Wt::WDate {1899, 1, 1} },		// missing month and days
		{ "1600",		Wt::WDate {1600, 1, 1} },		// missing month and days
		{ "1995/05-09",	Wt::WDate {} },					// invalid mixup separators
		{ "1995-05/09",	Wt::WDate {} },					// invalid mixup separators
	};

	for (const TestCase& testCase : testCases)
	{
		const Wt::WDate parsed {parseDate(testCase.str)};

		EXPECT_EQ(parsed.year(), testCase.result.year()) << " str was '" << testCase.str << "'";
		EXPECT_EQ(parsed.month(), testCase.result.month()) << " str was '" << testCase.str << "'";
		EXPECT_EQ(parsed.day(), testCase.result.day()) << " str was '" << testCase.str << "'";
	}
}

TEST(MetaData, extractPerformerAndRole)
{
	using namespace MetaData::Utils;

	struct TestCase
	{
		std::string	str;
		std::string expectedArtistName;
		std::string expectedRole;
	} testCases []
	{
		{ "", "", "" },
		{ "(myrole)", "", "myrole" },
		{ "(my role)", "", "my role" },
		{ " ( my role ) ", "", "my role" },
		{ " (()) ", "", "()" },
		{ ")", ")", "" },
		{ "(", "(", "" },
		{ "artist name (my role)", "artist name", "my role" },
		{ "artist name ()", "artist name", "" },
		{ "artist name (  )", "artist name", "" },
		{ "artist (subname) name", "artist (subname) name", "" },
		{ " artist name  ( my role  )", "artist name", "my role" },
		{ "artist name (artist subname) (my role)", "artist name (artist subname)", "my role" },
		{ "artist name", "artist name", "" },
		{ "  artist name  ", "artist name", "" },
		{ "artist name (", "artist name (", "" },
		{ "artist name )", "artist name )", "" },
		{ "artist name (()", "artist name (", "" },
		{ "artist name (())", "artist name", "()" },
		{ "artist name ( () )", "artist name", "()" },
		{ "artist name (drums (drum set))", "artist name", "drums (drum set)" },
		{ "artist name (  drums (drum set) )", "artist name", "drums (drum set)" },
	};

	for (const TestCase& testCase : testCases)
	{
		PerformerArtist performer {extractPerformerAndRole(testCase.str)};

		EXPECT_EQ(performer.artist.name, testCase.expectedArtistName) << " str was '" << testCase.str << "'";
		EXPECT_EQ(performer.role, testCase.expectedRole) << " str was '" << testCase.str << "'";
	}
}

TEST(MetaData, primaryReleaseTypes)
{
	using namespace MetaData;

	struct TestCase
	{
		std::string	str;
		std::optional<Release::PrimaryType> result;
	} testCases []
	{
		{ "", 			std::nullopt },
		{ "album",		Release::PrimaryType::Album },
		{ "Album",		Release::PrimaryType::Album },
		{ " Album", 	Release::PrimaryType::Album },
		{ "Album ", 	Release::PrimaryType::Album },
		{ "ep",			Release::PrimaryType::EP },
		{ " ep ",		Release::PrimaryType::EP },
		{ "broadcast",	Release::PrimaryType::Broadcast },
		{ "single",		Release::PrimaryType::Single },
		{ "other",		Release::PrimaryType::Other },
	};

	for (const TestCase& testCase : testCases)
	{
		std::optional<Release::PrimaryType> parsed {StringUtils::readAs<Release::PrimaryType>(testCase.str)};

		EXPECT_EQ(parsed, testCase.result) << " str was '" << testCase.str << "'";
	}
}

TEST(MetaData, secondaryReleaseTypes)
{
	using namespace MetaData;

	struct TestCase
	{
		std::string	str;
		std::optional<Release::SecondaryType> result;
	} testCases []
	{
		{ "", 				std::nullopt },
		{ "compilation",	Release::SecondaryType::Compilation },
		{ " compilation ",	Release::SecondaryType::Compilation },
		{ "soundtrack",		Release::SecondaryType::Soundtrack },
		{ "live", 			Release::SecondaryType::Live },
		{ "demo", 			Release::SecondaryType::Demo },
	};

	for (const TestCase& testCase : testCases)
	{
		std::optional<Release::SecondaryType> parsed {StringUtils::readAs<Release::SecondaryType>(testCase.str)};

		EXPECT_EQ(parsed, testCase.result) << " str was '" << testCase.str << "'";
	}
}

