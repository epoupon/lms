/*
 * Copyright (C) 2013 Emeric Poupon
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
#include <sstream>
#include <iomanip>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include "Utils.hpp"

template<>
boost::optional<Wt::WDate> readAs(const std::string& str)
{
	const std::vector<std::string> formats = {
		"yyyy-MM-dd",
		"yyyy/MM/dd",
		"yyyy-MM",
		"yyyy/MM",
		"yyyy"
	};

	for (auto format : formats)
	{
		auto date = Wt::WDate::fromString(str, format);

		if (!date.isValid())
			continue;

		return date;
	}

	return boost::none;
}

bool readList(const std::string& str, const std::string& separators, std::list<std::string>& results)
{
	std::string curStr;

	for (char c : str)
	{
		if (separators.find(c) != std::string::npos) {
			if (!curStr.empty()) {
				results.push_back(curStr);
				curStr.clear();
			}
		}
		else {
			if (curStr.empty() && std::isspace(c))
				continue;

			curStr.push_back(c);
		}
	}

	if (!curStr.empty())
		results.push_back(curStr);

	return !str.empty();
}

std::string
durationToString(boost::posix_time::time_duration duration, std::string format)
{
	boost::posix_time::time_facet* facet = new boost::posix_time::time_facet();
	facet->time_duration_format(format.c_str());
	std::ostringstream oss;
	oss.imbue(std::locale(oss.getloc(), facet));
	oss << duration;

	return oss.str();
}

std::vector<std::string>
splitString(std::string string, std::string separators)
{
	std::vector<std::string> res;

	boost::algorithm::split(res, string, boost::is_any_of(separators), boost::token_compress_on);

	return res;
}

std::string
stringTrim(const std::string& str, const std::string& whitespace)
{
	const auto strBegin = str.find_first_not_of(whitespace);
	if (strBegin == std::string::npos)
		return ""; // no content

	const auto strEnd = str.find_last_not_of(whitespace);
	const auto strRange = strEnd - strBegin + 1;

	return str.substr(strBegin, strRange);
}

std::string
stringTrimEnd(const std::string& str, const std::string& whitespace)
{
	return str.substr(0, str.find_last_not_of(whitespace)+1);
}

std::string
bufferToString(const std::vector<unsigned char>& data)
{
	std::ostringstream oss;

	for (unsigned char c : data)
	{
		oss << std::setw(2) << std::setfill('0') << std::hex << (int)c;
	}

	return oss.str();
}

