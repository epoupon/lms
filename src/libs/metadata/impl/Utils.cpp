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

namespace MetaData::Utils
{
	Wt::WDate
	parseDate(const std::string& dateStr)
	{
		static constexpr const char* formats[]
		{
			"%Y-%m-%d",
			"%Y/%m/%d",
		};

		for (const char* format : formats)
		{
			std::tm tm = {};
			std::stringstream ss {dateStr};
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
}

