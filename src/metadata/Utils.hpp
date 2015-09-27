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

#ifndef METADATA_UTILS_HPP
#define METADATA_UTILS_HPP

#include <string>
#include <list>

#include <boost/locale.hpp>

namespace MetaData
{

bool readAsPosixTime(const std::string& str, boost::posix_time::ptime& time);

bool readList(const std::string& str, const std::string& separators, std::list<std::string>& results);

template<typename T>
static inline bool readAs(const std::string& str, T& data)
{
	std::istringstream iss ( str );
	iss >> data;
	return !iss.fail();
}



std::string
static inline string_trim(const std::string& str,
		const std::string& whitespace = " \t")
{
	const auto strBegin = str.find_first_not_of(whitespace);
	if (strBegin == std::string::npos)
		return ""; // no content

	const auto strEnd = str.find_last_not_of(whitespace);
	const auto strRange = strEnd - strBegin + 1;

	return str.substr(strBegin, strRange);
}


std::string
static inline string_to_utf8(const std::string& str)
{
	return boost::locale::conv::to_utf<char>(str, "UTF-8");
}

} // namespace MetaData

#endif

