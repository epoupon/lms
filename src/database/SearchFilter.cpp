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

#include "SearchFilter.hpp"

namespace Database
{

SqlQuery generatePartialQuery(SearchFilter& filter)
{
	SqlQuery sqlQuery;

	// Process name like parameters
	for (auto nameLikeMatches : filter.nameLikeMatch)
	{
		WhereClause likeWhereClause;

		for (auto nameLikeMatch : nameLikeMatches)
		{

			// Artist
			switch (nameLikeMatch.first)
			{
				case SearchFilter::Field::Artist:
					for (const std::string& name : nameLikeMatch.second)
						likeWhereClause.Or( WhereClause("a.name LIKE ?") ).bind("%%" + name + "%%");
					break;

				case SearchFilter::Field::Release:
					for (const std::string& name : nameLikeMatch.second)
						likeWhereClause.Or( WhereClause("r.name LIKE ?") ).bind("%%" + name + "%%");
					break;

				case SearchFilter::Field::Genre:
					for (const std::string& name : nameLikeMatch.second)
						likeWhereClause.Or( WhereClause("g.name LIKE ?") ).bind("%%" + name + "%%");
					break;

				case SearchFilter::Field::Track:
					for (const std::string& name : nameLikeMatch.second)
						likeWhereClause.Or( WhereClause("t.name LIKE ?") ).bind("%%" + name + "%%");
					break;
			}
		}
		sqlQuery.where().And( likeWhereClause );
	}

	// Process id exact match parameters

	for (auto idMatch : filter.idMatch)
	{
		WhereClause idWhereClause;

		switch (idMatch.first)
		{
			case SearchFilter::Field::Artist:
				for (auto id : idMatch.second)
					idWhereClause.Or( WhereClause("a.id = ?") ).bind( std::to_string(id));
				break;
			case SearchFilter::Field::Release:
				for (auto id : idMatch.second)
					idWhereClause.Or( WhereClause("r.id = ?") ).bind( std::to_string(id));
				break;
			case SearchFilter::Field::Genre:
				for (auto id : idMatch.second)
					idWhereClause.Or( WhereClause("g.id = ?") ).bind( std::to_string(id));
				break;
			case SearchFilter::Field::Track:
				for (auto id : idMatch.second)
					idWhereClause.Or( WhereClause("t.id = ?") ).bind( std::to_string(id));
				break;
		}

		sqlQuery.where().And( idWhereClause );
	}

	return sqlQuery;
}

} // namespace Database

