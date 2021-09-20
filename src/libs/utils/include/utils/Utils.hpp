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

#include <algorithm>
#include <functional>

namespace Utils
{

	template<class T, class Compare = std::less<>>
	constexpr T clamp(T v, T lo, T hi, Compare comp = {})
	{
		assert(!comp(hi, lo));
		return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
	}

	template <typename Container, typename T>
	void
	push_back_if_not_present(Container& container, const T& val)
	{
		if (std::find(std::cbegin(container), std::cend(container), val) == std::cend(container))
			container.push_back(val);
	}

}
