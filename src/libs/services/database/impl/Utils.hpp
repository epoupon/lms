/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "services/database/Types.hpp"

namespace Database::Utils
{
#define ESCAPE_CHAR_STR "\\"
	static inline constexpr char escapeChar {'\\'};
	std::string escapeLikeKeyword(std::string_view keywords);

	template <typename T>
	RangeResults<T>
	execQuery(Wt::Dbo::Query<T>& query, Range range)
	{
		RangeResults<T> res;

		auto collection {query.limit(range.size ? static_cast<int>(range.size) + 1 : -1)
							.offset(range.offset ? static_cast<int>(range.offset) : -1)
							.resultList()};

		res.results.assign(collection.begin(), collection.end());
		if (range.size && res.results.size() == static_cast<std::size_t>(range.size) + 1)
		{
			res.moreResults = true;
			res.results.pop_back();
		}
		else
			res.moreResults = false;

		res.range.offset = range.offset;
		res.range.size = res.results.size();
		return res;
	}

	template <typename T>
	RangeResults<typename T::pointer>
	execQuery(Wt::Dbo::Query<Wt::Dbo::ptr<T>>& query, Range range)
	{
		RangeResults<typename T::pointer> res;

		auto collection {query.limit(range.size ? static_cast<int>(range.size) + 1 : -1)
							.offset(range.offset ? static_cast<int>(range.offset) : -1)
							.resultList()};

		res.results.assign(collection.begin(), collection.end());
		if (range.size && res.results.size() == static_cast<std::size_t>(range.size) + 1)
		{
			res.moreResults = true;
			res.results.pop_back();
		}
		else
			res.moreResults = false;

		res.range.offset = range.offset;
		res.range.size = res.results.size();
		return res;
	}

	Wt::WDateTime normalizeDateTime(const Wt::WDateTime& dateTime);
} // namespace Database::Utils

