/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <boost/foreach.hpp>

#include "AudioTypes.hpp"

#include "SqlQuery.hpp"

namespace Database
{

Artist::Artist(const std::string& name)
: _name(std::string(name, 0 , _maxNameLength))
{
}

// Accesoors
Artist::pointer
Artist::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	return session.find<Artist>().where("name = ?").bind( std::string(name, 0, _maxNameLength) );
}

// Create
Artist::pointer
Artist::create(Wt::Dbo::Session& session, const std::string& name)
{
	return session.add( new Artist( name ) );
}

Artist::pointer
Artist::getNone(Wt::Dbo::Session& session)
{
	pointer res = getByName(session, "<None>");
	if (!res)
		res = create(session, "<None>");

	return res;
}

Wt::Dbo::collection<Artist::pointer>
Artist::getAll(Wt::Dbo::Session& session, int offset, int size)
{
	return session.find<Artist>().offset(offset).limit(size);
}

Wt::Dbo::collection<Artist::pointer>
Artist::getAllOrphans(Wt::Dbo::Session& session)
{
	return session.query< Wt::Dbo::ptr<Artist> >("select a from artist a LEFT OUTER JOIN Track t ON a.id = t.artist_id WHERE t.id IS NULL");
}

Wt::Dbo::collection<Artist::pointer>
Artist::getAll(Wt::Dbo::Session& session,
		const std::vector<Genre::id_type>& genreIds,
		int offset, int size)
{
	std::string sqlQuery = "SELECT a FROM artist a";

	if (!genreIds.empty())
	{
		sqlQuery += " INNER JOIN genre g ON g.id = t_g.genre_id";
		sqlQuery += " INNER JOIN track t ON t.id = t_g.track_id AND t.artist_id = a.id";
		sqlQuery += " INNER JOIN track_genre t_g ON t_g.track_id = t.id AND t_g.genre_id = g.id";
	}

	WhereClause where;
	{
		WhereClause genreWhere;

		for (std::size_t i = 0; i < genreIds.size(); ++i)
			genreWhere.Or( WhereClause("g.id = ?") );

		where.And(genreWhere);
	}

	Wt::Dbo::Query<Artist::pointer> query = session.query<Artist::pointer>( sqlQuery + " " + where.get() ).offset(offset).limit(size);

	BOOST_FOREACH(const Genre::id_type genreId, genreIds)
		query.bind(genreId);

	query.groupBy("a");

	return query;
}


} // namespace Database

