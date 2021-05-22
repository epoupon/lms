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

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>

#define QUOTEME(x) QUOTEME_1(x)
#define QUOTEME_1(x) #x

namespace StringUtils {

std::vector<std::string>
splitStringCopy(std::string_view string, std::string_view separators);

std::vector<std::string_view>
splitString(std::string_view string, std::string_view separators);

std::string
joinStrings(const std::vector<std::string>& strings, const std::string& delimiter);

std::string
stringTrim(std::string_view str, std::string_view whitespaces = " \t");

std::string
stringTrimEnd(std::string_view str, std::string_view whitespaces = " \t");

std::string
stringToLower(std::string_view str);

void
stringToLower(std::string& str);

std::string
stringToUpper(const std::string& str);

std::string
bufferToString(const std::vector<unsigned char>& data);

template<typename T>
std::optional<T> readAs(std::string_view str)
{
	T res;

	std::istringstream iss {std::string {str}};
	iss >> res;
	if (iss.fail())
		return std::nullopt;

	return res;
}

template<>
std::optional<std::string>
readAs(std::string_view str);

std::string
replaceInString(const std::string& str, const std::string& from, const std::string& to);

std::string
jsEscape(const std::string& str);

std::string
escapeString(std::string_view str, std::string_view charsToEscape, char escapeChar);

bool
stringEndsWith(const std::string& str, const std::string& ending);

std::optional<std::string>
stringFromHex(const std::string& str);

} // StringUtils

