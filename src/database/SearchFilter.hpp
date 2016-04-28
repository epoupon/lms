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

class SearchFilter
{
	public:

		enum class Field {
			Artist,		// artist
			Release,	// release
			Track,		// track
			Cluster,	// cluster
		};

		typedef std::vector<std::map<Field, std::vector<std::string> > > NameLikeMatchType;
		typedef std::map<Field, std::vector< Wt::Dbo::dbo_default_traits::IdType> > IdMatchType;

		SearchFilter() {}

		// Helpers
		static SearchFilter IdMatch( const IdMatchType& _idMatch )
		{
			return SearchFilter(_idMatch);
		}

		static SearchFilter NameLikeMatch( const NameLikeMatchType& _nameLikeMatch )
		{
			return SearchFilter(_nameLikeMatch);
		}

		// Single Field ID match
		static SearchFilter ById(Field field, Wt::Dbo::dbo_default_traits::IdType id)
		{
			return SearchFilter({{field, {id} }});
		}

		// Single Field Name match OR
		static SearchFilter ByNameOr(Field field, std::vector<std::string> keywords)
		{
			return NameLikeMatch({{{field, keywords}}});
		}

		// Single Field Name match AND
		static SearchFilter ByNameAnd(Field field, std::vector<std::string> keywords)
		{
			NameLikeMatchType nameLikeMatch;

			for (auto keyword : keywords)
				nameLikeMatch.push_back({{{field, {keyword}}}});

			return NameLikeMatch(nameLikeMatch);
		}

		// The filter is a AND of the following conditions:

		// ((Field1.name LIKE STR1-1 OR Field1.name LIKE STR1-2 ...) OR (Field2.name LIKE STR2-1 OR Field2.name LIKE STR2-2 ...) ...
		NameLikeMatchType	nameLikeMatch;

		// (Field1.id IN (ID1-1,ID1-2 ... ) AND (Field2.id IN (ID2-1,ID2-2 ... ) ...
		IdMatchType	idMatch;

	private:

		SearchFilter(const NameLikeMatchType& _nameLikeMatch) : nameLikeMatch(_nameLikeMatch) {}
		SearchFilter(const IdMatchType& _idMatch) : idMatch(_idMatch) {}
};

SqlQuery generatePartialQuery(SearchFilter& filter);

} // namespace Database

#endif // _DB_SEARCH_FILTER_HPP_
