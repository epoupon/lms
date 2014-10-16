/*
 * Copyright (C) 2014 Emeric Poupon
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

#ifndef FILTER_CHAIN_HPP
#define FILTER_CHAIN_HPP

#include <Wt/WSignal>

#include "Filter.hpp"
#include "KeywordSearchFilter.hpp"

namespace UserInterface {

// FilterChain
class FilterChain
{
	public:

		FilterChain();

		void addFilter(Filter* filter);

		// First filter is a keywork search
		void searchKeyword(const std::string& text);

		// Update filters from filter @ startIdx
		void updateFilters(std::size_t startIdx);

	private:

		KeywordSearchFilter	_keywordSearchFilter;

		// No ownership
		std::vector<Filter*>	_filters;

		bool _refreshingFilters;
};

} // namespace UserInterface

#endif
