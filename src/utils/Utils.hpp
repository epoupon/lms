/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <chrono>
#include <list>
#include <string>
#include <sstream>
#include <vector>

#include <boost/optional.hpp>

#include <Wt/WDate.h>

bool
readList(const std::string& str, const std::string& separators, std::list<std::string>& results);

std::vector<std::string>
splitString(std::string string, std::string separators);

std::string
stringTrim(const std::string& str, const std::string& whitespaces = " \t");

std::string
stringTrimEnd(const std::string& str, const std::string& whitespaces = " \t");

std::string
bufferToString(const std::vector<unsigned char>& data);

template<typename T>
boost::optional<T> readAs(const std::string& str)
{
	T res;

	std::istringstream iss ( str );
	iss >> res;
	if (iss.fail())
		return boost::none;

	return res;
}

