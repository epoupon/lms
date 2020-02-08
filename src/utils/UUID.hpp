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

#include <string>
#include <string_view>

#include "Utils.hpp"

class UUID
{
	public:

		std::string_view getAsString() const { return _value; }

	private:

		template <typename UUID>
		friend std::optional<UUID> readAs(const std::string& str);

		UUID(std::string_view value) : _value {value} {}
		std::string _value;
};

template<>
std::optional<UUID>
readAs(const std::string& str);

