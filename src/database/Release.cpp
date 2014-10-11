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

#include "SqlQuery.hpp"

#include "AudioTypes.hpp"

namespace Database {

Release::Release(const std::string& name)
: _name(std::string(name, 0, _maxNameLength))
{
}

Release::pointer
Release::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	return session.find<Release>().where("name = ?").bind( std::string(name, 0, _maxNameLength) );
}

Release::pointer
Release::getById(Wt::Dbo::Session& session, id_type id)
{
	return session.find<Release>().where("id = ?").bind(id);
}

Release::pointer
Release::getNone(Wt::Dbo::Session& session)
{
	pointer res = getByName(session, "<None>");
	if (!res)
		res = create(session, "<None>");

	return res;
}

Release::pointer
Release::create(Wt::Dbo::Session& session, const std::string& name)
{
	return session.add(new Release(name));
}

Wt::Dbo::collection<Release::pointer>
Release::getAll(Wt::Dbo::Session& session,
			const std::vector<Artist::id_type>& artistIds,
			const std::vector<Genre::id_type>& genreIds,
			int offset, int size)
{
	std::string sqlQuery = "SELECT r FROM release r";

	if (!artistIds.empty() || !genreIds.empty())
		sqlQuery += " INNER JOIN track t ON t.release_id = r.id";

	if (!artistIds.empty())
	{
		sqlQuery += " INNER JOIN artist a ON a.id = t.artist_id";
	}

	if (!genreIds.empty())
	{
		sqlQuery += " INNER JOIN genre g ON g.id = t_g.genre_id";
		sqlQuery += " INNER JOIN track_genre t_g ON t_g.track_id = t.id AND t_g.genre_id = g.id";
	}

	WhereClause where;
	{
		WhereClause artistWhere;

		for (std::size_t i = 0; i < artistIds.size(); ++i)
			artistWhere.Or( WhereClause("a.id = ?") );

		where.And(artistWhere);
	}

	{
		WhereClause genreWhere;

		for (std::size_t i = 0; i < genreIds.size(); ++i)
			genreWhere.Or( WhereClause("g.id = ?") );

		where.And(genreWhere);
	}

	Wt::Dbo::Query<Release::pointer> query = session.query<Release::pointer>( sqlQuery + " " + where.get() ).offset(offset).limit(size);

	BOOST_FOREACH(const Artist::id_type artistId, artistIds)
		query.bind(artistId);

	BOOST_FOREACH(const Genre::id_type genreId, genreIds)
		query.bind(genreId);

	query.groupBy("r");

	return query;
}

boost::posix_time::time_duration
Release::getDuration(void) const
{
	typedef Wt::Dbo::collection< Wt::Dbo::ptr<Track> > Tracks;

	boost::posix_time::time_duration res;

	for (Tracks::const_iterator it = _tracks.begin(); it != _tracks.end(); ++it)
		res += (*it)->getDuration();

	return res;
}

Wt::Dbo::collection<Release::pointer>
Release::getAllOrphans(Wt::Dbo::Session& session)
{
	return session.query< Wt::Dbo::ptr<Release> >("select r from release r LEFT OUTER JOIN Track t ON r.id = t.release_id WHERE t.id IS NULL");
}



} // namespace Database
