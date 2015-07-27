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

#include "Types.hpp"
#include "SqlQuery.hpp"

#include "logger/Logger.hpp"

namespace Database
{

Artist::Artist(const std::string& name, const std::string& MBID)
: _name(std::string(name, 0 , _maxNameLength)),
_MBID(MBID)
{

}

std::vector<Artist::pointer>
Artist::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	Wt::Dbo::collection<Artist::pointer> res = session.find<Artist>().where("name = ?").bind( std::string(name, 0, _maxNameLength) );
	return std::vector<Artist::pointer>(res.begin(), res.end());
}

Artist::pointer
Artist::getByMBID(Wt::Dbo::Session& session, const std::string& mbid)
{
	return session.find<Artist>().where("mbid = ?").bind(mbid);
}

Artist::pointer
Artist::create(Wt::Dbo::Session& session, const std::string& name, const std::string& MBID)
{
	return session.add(new Artist(name, MBID));
}

Artist::pointer
Artist::getNone(Wt::Dbo::Session& session)
{
	std::vector<pointer> res = getByName(session, "<None>");
	if (res.empty())
		return create(session, "<None>");

	return res.front();
}

std::vector<Artist::pointer>
Artist::getAll(Wt::Dbo::Session& session, int offset, int size)
{
	Wt::Dbo::collection<pointer> res = session.find<Artist>().offset(offset).limit(size);
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Artist::pointer>
Artist::getAllOrphans(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<Artist::pointer> res = session.query< Wt::Dbo::ptr<Artist> >("select a from artist a LEFT OUTER JOIN Track t ON a.id = t.artist_id WHERE t.id IS NULL");

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Artist::pointer>
Artist::getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	Wt::Dbo::collection<Artist::pointer> res = getQuery(session, filter).limit(size).offset(offset);

	return std::vector<pointer>(res.begin(), res.end());
}

Wt::Dbo::Query<Artist::pointer>
Artist::getQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<pointer> query
		= session.query<pointer>( "SELECT a FROM artist a INNER JOIN track t ON t.artist_id = a.id INNER JOIN release r ON r.id = t.release_id INNER JOIN genre g ON g.id = t_g.genre_id INNER JOIN track_genre t_g ON t_g.track_id = t.id " + sqlQuery.where().get()).groupBy("a.id").orderBy("a.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Wt::Dbo::Query<Artist::UIQueryResult>
Artist::getUIQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<UIQueryResult> query
		= session.query<UIQueryResult>( "SELECT a.id, a.name, COUNT(DISTINCT r.id), COUNT(DISTINCT t.id) FROM artist a INNER JOIN track t ON t.artist_id = a.id INNER JOIN release r ON r.id = t.release_id INNER JOIN genre g ON g.id = t_g.genre_id INNER JOIN track_genre t_g ON t_g.track_id = t.id " + sqlQuery.where().get()).groupBy("a.id").orderBy("a.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

void
Artist::updateUIQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel<UIQueryResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query<UIQueryResult> query = getUIQuery(session, filter);

	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 3)
	{
		model.addColumn( "a.name", columnNames.at(0));
		model.addColumn( "COUNT(DISTINCT r.id)", columnNames.at(1) );
		model.addColumn( "COUNT(DISTINCT t.id)", columnNames.at(2) );
	}
}


} // namespace Database
