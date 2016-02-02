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

#include <Wt/Dbo/QueryModel>

#include "logger/Logger.hpp"

#include "SqlQuery.hpp"

#include "Types.hpp"

namespace Database {

Track::Track(const boost::filesystem::path& p)
:
_trackNumber(0),
_totalTrackNumber(0),
_discNumber(0),
_totalDiscNumber(0),
_filePath( p.string() ),
_coverType(CoverType::None)
{
}

Wt::Dbo::collection< Track::pointer >
Track::getAll(Wt::Dbo::Session& session)
{
	return session.find<Track>();
}

void
Track::setGenres(std::vector<Genre::pointer> genres)
{
	if (_genres.size())
		_genres.clear();

	for (Genre::pointer genre : genres) {
		_genres.insert( genre );
	}
}

Track::pointer
Track::getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.find<Track>().where("file_path = ?").bind(p.string());
}

Track::pointer
Track::getById(Wt::Dbo::Session& session, id_type id)
{
	return session.find<Track>().where("id = ?").bind(id);
}

Track::pointer
Track::getByMBID(Wt::Dbo::Session& session, const std::string& mbid)
{
	return session.find<Track>().where("mbid = ?").bind(mbid);
}

Track::pointer
Track::create(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.add(new Track(p) );
}

std::vector<boost::filesystem::path>
Track::getAllPaths(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction(session);
	Wt::Dbo::collection<std::string> res = session.query<std::string>("SELECT file_path from track");
	return std::vector<boost::filesystem::path>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getMBIDDuplicates(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.query<pointer>( "SELECT track FROM track WHERE mbid in (SELECT mbid FROM track WHERE mbid <> '' GROUP BY mbid HAVING COUNT (*) > 1)").orderBy("track.release_id,track.disc_number,track.track_number,track.mbid");
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getChecksumDuplicates(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.query<pointer>( "SELECT track FROM track WHERE checksum in (SELECT checksum FROM track WHERE Length(checksum) > 0 GROUP BY checksum HAVING COUNT(*) > 1)").orderBy("track.release_id,track.disc_number,track.track_number,track.checksum");
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector< Genre::pointer >
Track::getGenres(void) const
{
	std::vector< Genre::pointer > genres;
	std::copy(_genres.begin(), _genres.end(), std::back_inserter(genres));
	return genres;
}

Wt::Dbo::Query< Track::pointer >
Track::getQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<pointer> query
		= session.query<pointer>( "SELECT t FROM track t INNER JOIN artist a ON t.artist_id = a.id INNER JOIN genre g ON g.id = t_g.genre_id INNER JOIN track_genre t_g ON t_g.track_id = t.id INNER JOIN release r ON r.id = t.release_id " + sqlQuery.where().get()).groupBy("t.id").orderBy("a.name,t.date,r.name,t.disc_number,t.track_number");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Wt::Dbo::Query< Track::UIQueryResult >
Track::getUIQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<UIQueryResult> query
		= session.query<UIQueryResult>( "SELECT t.id, a.name, r.name, t.disc_number, t.track_number, t.name, t.duration, t.date, t.original_date, t.genre_list FROM track t INNER JOIN artist a ON t.artist_id = a.id INNER JOIN genre g ON g.id = t_g.genre_id INNER JOIN track_genre t_g ON t_g.track_id = t.id INNER JOIN release r ON r.id = t.release_id " + sqlQuery.where().get()).groupBy("t.id").orderBy("a.name,t.date,r.name,t.disc_number,t.track_number");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Track::StatsQueryResult
Track::getStats(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<StatsQueryResult> query = session.query<StatsQueryResult>( "SELECT COUNT(\"id\"), SUM(\"dur\") FROM  (SELECT t.id as \"id\", t.duration as \"dur\" FROM track t INNER JOIN artist a ON t.artist_id = a.id INNER JOIN genre g ON g.id = t_g.genre_id INNER JOIN track_genre t_g ON t_g.track_id = t.id INNER JOIN release r ON r.id = t.release_id " + sqlQuery.where().get() + " GROUP BY t.id)");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}


std::vector<Track::pointer>
Track::getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	Wt::Dbo::collection<pointer> res = getQuery(session, filter).limit(size).offset(offset);

	return std::vector<pointer>(res.begin(), res.end());
}

void
Track::updateUIQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel< UIQueryResult >& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query< UIQueryResult > query = getUIQuery(session, filter);
	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 9)
	{
		model.addColumn( "a.name", columnNames[0] );
		model.addColumn( "r.name", columnNames[1] );
		model.addColumn( "t.disc_number", columnNames[2] );
		model.addColumn( "t.track_number", columnNames[3] );
		model.addColumn( "t.name", columnNames[4] );
		model.addColumn( "t.duration", columnNames[5] );
		model.addColumn( "t.date", columnNames[6] );
		model.addColumn( "t.original_date", columnNames[7] );
		model.addColumn( "t.genre_list", columnNames[8] );
	}

}


Genre::Genre()
{
}

Genre::Genre(const std::string& name)
: _name( std::string(name, 0, _maxNameLength) )
{
}

Wt::Dbo::collection<Genre::pointer>
Genre::getAll(Wt::Dbo::Session& session)
{
	return session.find<Genre>();
}

Genre::pointer
Genre::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	// TODO use like search
	return session.find<Genre>().where("name = ?").bind( std::string(name, 0, _maxNameLength) );
}

Genre::pointer
Genre::getNone(Wt::Dbo::Session& session)
{
	pointer res = getByName(session, "<None>");
	if (!res)
		res = create(session, "<None>");
	return res;
}

bool
Genre::isNone(void) const
{
	return (_name == "<None>");
}

Genre::pointer
Genre::create(Wt::Dbo::Session& session, const std::string& name)
{
	return session.add(new Genre(name));
}

Wt::Dbo::Query<Genre::pointer>
Genre::getQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<pointer> query
		= session.query<pointer>( "SELECT g FROM genre g INNER JOIN track_genre t_g ON t_g.genre_id = g.id INNER JOIN artist a ON t.artist_id = a.id INNER JOIN release r ON r.id = t.release_id INNER JOIN track t ON t.id = t_g.track_id " + sqlQuery.where().get()).groupBy("g.name").orderBy("g.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Wt::Dbo::Query<Genre::UIQueryResult>
Genre::getUIQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<UIQueryResult> query
		= session.query<UIQueryResult>( "SELECT g.id, g.name, COUNT(DISTINCT t.id) FROM genre g INNER JOIN track_genre t_g ON t_g.genre_id = g.id INNER JOIN artist a ON t.artist_id = a.id INNER JOIN release r ON r.id = t.release_id INNER JOIN track t ON t.id = t_g.track_id " + sqlQuery.where().get()).groupBy("g.name").orderBy("g.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

void
Genre::updateUIQueryModel(Wt::Dbo::Session& session,  Wt::Dbo::QueryModel<UIQueryResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query<UIQueryResult> query = getUIQuery(session, filter);
	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 2)
	{
		model.addColumn( "g.name", columnNames[0] );
		model.addColumn( "COUNT(DISTINCT t.id)", columnNames[1] );
	}
}

std::vector<Genre::pointer>
Genre::getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	Wt::Dbo::collection<pointer> res = getQuery(session, filter).limit(size).offset(offset);

	return std::vector<pointer>(res.begin(), res.end());
}

} // namespace Database

