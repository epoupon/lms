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

#ifndef _DB_SEARCH_FILTER_HPP_
#define _DB_SEARCH_FILTER_HPP_

#include <string>
#include <vector>
#include <map>

#include <Wt/Dbo/Dbo>

#include "SqlQuery.hpp"

namespace Database
{

typedef Wt::Dbo::dbo_default_traits::IdType id_type;

class SearchFilter
{
	public:

		typedef int id_type;

		SearchFilter() {}

		static SearchFilter Artist(std::string name) {return SearchFilter();}
		static SearchFilter Artist(id_type id) {return SearchFilter();}

		static SearchFilter Release(std::string name) {return SearchFilter();}
		static SearchFilter Release(id_type id) {return SearchFilter();}

		static SearchFilter Track(std::string name) {return SearchFilter();}

		static SearchFilter Cluster(id_type id) {return SearchFilter();}

		// Combine search filters by add operation
		// Caution: multiple filters on different artist/release/track
		// values may lead to empty results
		void operator+=(const SearchFilter& filter);

		SqlQuery generatePartialQuery();

	private:

};


} // namespace Database

#endif // _DB_SEARCH_FILTER_HPP_
