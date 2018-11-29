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

#include "Utils.hpp"

#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

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

std::vector<std::string>
splitString(std::string string, std::string separators)
{
	std::vector<std::string> res;

	boost::algorithm::split(res, string, boost::is_any_of(separators), boost::token_compress_on);

	return res;
}

std::string
joinStrings(std::vector<std::string> strings, std::string delimiter)
{
	return boost::algorithm::join(strings, delimiter);
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

std::string
replaceInString(std::string str, const std::string& from, const std::string& to)
{
    size_t pos = 0;

    while ((pos = str.find(from, pos)) != std::string::npos)
    {
         str.replace(pos, from.length(), to);
         pos += to.length();
    }

    return str;
}


