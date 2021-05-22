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

#include "utils/UUID.hpp"

#include <regex>

#include "utils/String.hpp"

namespace StringUtils
{
	template <>
	std::optional<UUID>
	readAs(std::string_view str)
	{
		return UUID::fromString(str);
	}
}

static
bool
stringIsUUID(std::string_view str)
{
	static const std::regex re { R"([0-9a-fA-F]{8}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{12})"};

	return std::regex_match(std::cbegin(str), std::cend(str), re);
}

UUID::UUID(std::string_view str)
	: _value {StringUtils::stringToLower(str)}
{
}

std::optional<UUID>
UUID::fromString(std::string_view str)
{
	if (!stringIsUUID(str))
		return std::nullopt;

	return UUID {str};
}

