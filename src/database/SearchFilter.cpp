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

static std::ostream& operator<<(std::ostream& ost, const std::vector< Wt::Dbo::dbo_default_traits::IdType>& ids)
{
	const char *sep = "";

	for (auto id : ids)
	{
		ost << sep << std::to_string(id);
		sep = ",";
	}

	return ost;
}

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
				{
					std::ostringstream oss;
					oss << "a.id IN (" << idMatch.second << ")";
					idWhereClause.Or(oss.str());
				}
				break;
			case SearchFilter::Field::Release:
				{
					std::ostringstream oss;
					oss << "r.id IN (" << idMatch.second << ")";
					idWhereClause.Or(oss.str());
				}
				break;
			case SearchFilter::Field::Genre:
				{
					std::ostringstream oss;
					oss << "g.id IN (" << idMatch.second << ")";
					idWhereClause.Or(oss.str());
				}
				break;
			case SearchFilter::Field::Track:
				{
					std::ostringstream oss;
					oss << "t.id IN (" << idMatch.second << ")";
					idWhereClause.Or(oss.str());
				}
				break;
		}

		sqlQuery.where().And( idWhereClause );
	}

	return sqlQuery;
}

} // namespace Database

