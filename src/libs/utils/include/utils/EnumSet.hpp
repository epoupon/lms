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

template <typename T>
class EnumSet
{
	using UnderlyingType = std::uint64_t;

	public:
		constexpr EnumSet() = default;
		constexpr EnumSet(std::initializer_list<T> values)
		{
			for (auto value : values)
				insert(value);
		}

		constexpr void insert(T value)
		{
			_values |= (UnderlyingType{1} << static_cast<UnderlyingType>(value));
		}

		constexpr void erase(T value)
		{
			_values &= ~(UnderlyingType{1} << static_cast<UnderlyingType>(value));
		}

		constexpr bool contains(T value) const
		{
			return _values & (UnderlyingType{1} << static_cast<UnderlyingType>(value));
		}


	private:

		UnderlyingType _values {};
};

