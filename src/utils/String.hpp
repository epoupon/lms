/*
 * Copyright (C) 2020 Emeric Poupon
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

#pragma once

#include <optional>
#include <string>
#include <sstream>
#include <vector>

namespace StringUtils {

std::vector<std::string>
splitString(const std::string& string, const std::string& separators);

std::string
joinStrings(const std::vector<std::string>& strings, const std::string& delimiter);

std::string
stringTrim(const std::string& str, const std::string& whitespaces = " \t");

std::string
stringTrimEnd(const std::string& str, const std::string& whitespaces = " \t");

std::string
stringToLower(const std::string& str);

std::string
bufferToString(const std::vector<unsigned char>& data);

template<typename T>
std::optional<T> readAs(const std::string& str)
{
	T res;

	std::istringstream iss ( str );
	iss >> res;
	if (iss.fail())
		return std::nullopt;

	return res;
}

template<>
std::optional<std::string>
readAs(const std::string& str);

std::string
replaceInString(const std::string& str, const std::string& from, const std::string& to);

std::string
jsEscape(const std::string& str);

bool
stringEndsWith(const std::string& str, const std::string& ending);

std::optional<std::string>
stringFromHex(const std::string& str);

} // StringUtils

