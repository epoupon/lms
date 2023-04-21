/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "Utils.hpp"
#include <ctime>
#include <string_view>
#include <iomanip>
#include <sstream>

#include "utils/Exception.hpp"

namespace MetaData::Utils
{
	Wt::WDate
	parseDate(std::string_view dateStr)
	{
		static constexpr const char* formats[]
		{
			"%Y-%m-%d",
			"%Y/%m/%d",
		};

		for (const char* format : formats)
		{
			std::tm tm = {};
			std::istringstream ss {std::string {dateStr}}; // TODO, remove extra copy here
			ss >> std::get_time(&tm, format);
			if (ss.fail())
				continue;

			const Wt::WDate res
			{
				tm.tm_year + 1900,			// years since 1900
				tm.tm_mon + 1,				// months since January – [0, 11]
				tm.tm_mday ? tm.tm_mday : 1 // day of the month – [1, 31]
			};
			if (!res.isValid())
				continue;

			return res;
		}

		return {};
	}

	std::string_view
	readStyleToString(ParserReadStyle readStyle)
	{
		switch (readStyle)
		{
			case ParserReadStyle::Fast: return "fast";
			case ParserReadStyle::Average: return "average";
			case ParserReadStyle::Accurate: return "accurate";
		}

		throw LmsException {"Unknown read style"};
	}

	PerformerArtist
	extractPerformerAndRole(std::string_view entry)
	{
		std::string_view artistName;
		std::string_view role;

		std::size_t roleBegin {};
		std::size_t roleEnd {};
		std::size_t count {};

		for (std::size_t i {}; i < entry.size(); ++i)
		{
			std::size_t currentIndex {entry.size() - i - 1};
			const char c {entry[currentIndex]};

			if (std::isspace(c))
				continue;

			if (c == ')')
			{
				if (count++ == 0)
					roleEnd = currentIndex;
			}
			else if (c == '(')
			{
				if (count == 0)
					break;

				if (--count == 0)
				{
					roleBegin = currentIndex + 1;
					role = StringUtils::stringTrim(entry.substr(roleBegin, roleEnd - roleBegin));
					artistName = StringUtils::stringTrim(entry.substr(0, currentIndex));
					break;
				}
			}
			else if (count == 0)
				break;
		}

		if (!roleEnd || !roleBegin)
			artistName = StringUtils::stringTrim(entry);

		return PerformerArtist {Artist {artistName}, std::string {role}};
	}
}

namespace StringUtils
{
	static bool iequals(std::string_view a, std::string_view b)
	{
		return std::equal(std::cbegin(a), std::cend(a),
				std::cbegin(b), std::cend(b),
				[](char a, char b) { return tolower(a) == tolower(b);}
				);
	}

	template<>
	std::optional<MetaData::Release::PrimaryType> readAs(std::string_view str)
	{
		str = stringTrim(str);

		if (iequals(str, "album"))
			return MetaData::Release::PrimaryType::Album;
		else if (iequals(str, "single"))
			return MetaData::Release::PrimaryType::Single;
		else if (iequals(str, "EP"))
			return MetaData::Release::PrimaryType::EP;
		else if (iequals(str, "broadcast"))
			return MetaData::Release::PrimaryType::Broadcast;
		else if (iequals(str, "other"))
			return MetaData::Release::PrimaryType::Other;

		return std::nullopt;
	}

	template<>
	std::optional<MetaData::Release::SecondaryType> readAs(std::string_view str)
	{
		str = stringTrim(str);

		if (iequals(str, "compilation"))
			return MetaData::Release::SecondaryType::Compilation;
		else if (iequals(str, "soundtrack"))
			return MetaData::Release::SecondaryType::Soundtrack;
		else if (iequals(str, "live"))
			return MetaData::Release::SecondaryType::Live;
		else if (iequals(str, "demo"))
			return MetaData::Release::SecondaryType::Demo;

		return std::nullopt;
	}
}

