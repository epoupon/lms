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

#include <Wt/Dbo/QueryModel>

#include "logger/Logger.hpp"

#include "SqlQuery.hpp"

#include "Types.hpp"

namespace Database {

static SqlQuery generatePartialQuery(SearchFilter& filter, bool forceGenreInnerJoin = false)
{
	bool genreJoin = forceGenreInnerJoin;
	SqlQuery sqlQuery;


	// Process like searches
	BOOST_FOREACH(SearchFilter::FieldValues& likeMatch, filter.likeMatches)
	{
		WhereClause likeWhereClause;

		// Artist
		{
			WhereClause whereClause;

			BOOST_FOREACH(const std::string& name, likeMatch[SearchFilter::Field::Artist])
				whereClause.Or( WhereClause("t.artist_name LIKE ?") ).bind("%%" + name + "%%");

			likeWhereClause.Or( whereClause );
		}
		// Release
		{
			WhereClause whereClause;

			BOOST_FOREACH(const std::string& name, likeMatch[SearchFilter::Field::Release])
				whereClause.Or( WhereClause("t.release_name LIKE ?") ).bind("%%" + name + "%%");

			likeWhereClause.Or( whereClause );
		}
		// Genre
		{
			WhereClause whereClause;

			BOOST_FOREACH(const std::string& name, likeMatch[SearchFilter::Field::Genre])
			{
				whereClause.Or( WhereClause("g.name LIKE ?") ).bind("%%" + name + "%%");
				genreJoin = true;
			}

			likeWhereClause.Or( whereClause );
		}
		// Track
		{
			WhereClause whereClause;

			BOOST_FOREACH(const std::string& name, likeMatch[SearchFilter::Field::Track])
				whereClause.Or( WhereClause("t.name LIKE ?") ).bind("%%" + name + "%%");

			likeWhereClause.Or( whereClause );
		}

		sqlQuery.where().And( likeWhereClause );
	}

	// Add exact search constraints
	// Artist
	{
		WhereClause whereClause;

		BOOST_FOREACH(const std::string& name, filter.exactMatch[SearchFilter::Field::Artist])
			whereClause.Or( WhereClause("t.artist_name = ?") ).bind(name);

		sqlQuery.where().And( whereClause );
	}

	// Release
	{
		WhereClause whereClause;

		BOOST_FOREACH(const std::string& name, filter.exactMatch[SearchFilter::Field::Release])
			whereClause.Or( WhereClause("t.release_name = ?") ).bind(name);

		sqlQuery.where().And( whereClause );
	}

	// Genre
	{
		WhereClause whereClause;

		BOOST_FOREACH(const std::string& name, filter.exactMatch[SearchFilter::Field::Genre])
		{
			whereClause.Or( WhereClause("g.name = ?") ).bind(name);
			genreJoin = true;
		}

		sqlQuery.where().And( whereClause );
	}

	if (genreJoin)
	{
		sqlQuery.innerJoin().And( InnerJoinClause("genre g ON g.id = t_g.genre_id"));
		sqlQuery.innerJoin().And( InnerJoinClause("track_genre t_g ON t_g.track_id = t.id"));
	}

	return sqlQuery;
}


Track::Track(const boost::filesystem::path& p)
:
_trackNumber(0),
_discNumber(0),
_filePath( p.string() ),
_hasCover(false)
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

	BOOST_FOREACH(Genre::pointer genre, genres) {
		_genres.insert( genre );
	}
}

Track::pointer
Track::getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.find<Track>().where("path = ?").bind(p.string());
}

Track::pointer
Track::getById(Wt::Dbo::Session& session, id_type id)
{
	return session.find<Track>().where("id = ?").bind(id);
}


Track::pointer
Track::create(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.add(new Track(p) );
}

std::vector< Genre::pointer >
Track::getGenres(void) const
{
	std::vector< Genre::pointer > genres;
	std::copy(_genres.begin(), _genres.end(), std::back_inserter(genres));
	return genres;
}

Wt::Dbo::Query< Track::pointer >
Track::getAllQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<pointer> query
		= session.query<Track::pointer>( "SELECT t FROM track t " + sqlQuery.innerJoin().get() + " " + sqlQuery.where().get()).groupBy("t.id").orderBy("t.artist_name,t.date,t.release_name,t.disc_number,t.track_number");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Wt::Dbo::collection< Track::pointer >
Track::getAll(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	return getAllQuery(session, filter).limit(size).offset(offset);
}

std::vector<Track::pointer>
Track::getTracks(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	Wt::Dbo::collection< Track::pointer > tracks = getAll(session, filter, offset, size);

	return std::vector<Track::pointer>(tracks.begin(), tracks.end());
}

Wt::Dbo::Query<Track::ReleaseResult>
Track::getReleasesQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<ReleaseResult> query
		= session.query<ReleaseResult>("SELECT t.release_name, t.date, COUNT(DISTINCT t.id) FROM track t " + sqlQuery.innerJoin().get() + " " + sqlQuery.where().get()).groupBy("t.release_name").orderBy("t.release_name");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

void
Track::updateReleaseQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel<ReleaseResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query<ReleaseResult> query = getReleasesQuery(session, filter);

	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 3)
	{
		model.addColumn( "t.release_name", columnNames[0]);
		model.addColumn( "t.date", columnNames[1]);
		model.addColumn( "COUNT(DISTINCT t.id)", columnNames[2] );
	}
}

Wt::Dbo::Query<Track::ArtistResult>
Track::getArtistsQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<ArtistResult> query
		= session.query<ArtistResult>( "SELECT t.artist_name, COUNT(DISTINCT t.release_name), COUNT(DISTINCT t.id) FROM track t " + sqlQuery.innerJoin().get() + " " + sqlQuery.where().get()).groupBy("t.artist_name").orderBy("t.artist_name");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

void
Track::updateArtistQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel<ArtistResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query<ArtistResult> query = getArtistsQuery(session, filter);

	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 3)
	{
		model.addColumn( "t.artist_name", columnNames.at(0));
		model.addColumn( "COUNT(DISTINCT t.release_name)", columnNames.at(1) );
		model.addColumn( "COUNT(DISTINCT t.id)", columnNames.at(2) );
	}
}


void
Track::updateTracksQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel< pointer >& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query< pointer > query = getAllQuery(session, filter);
	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 9)
	{
		model.addColumn( "t.artist_name", columnNames[0] );
		model.addColumn( "t.release_name", columnNames[1] );
		model.addColumn( "t.disc_number", columnNames[2] );
		model.addColumn( "t.track_number", columnNames[3] );
		model.addColumn( "t.name", columnNames[4] );
		model.addColumn( "t.duration", columnNames[5] );
		model.addColumn( "t.date", columnNames[6] );
		model.addColumn( "t.original_date", columnNames[7] );
		model.addColumn( "t.genre_list", columnNames[8] );
	}

}

std::vector<std::string>
Track::getArtists(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<std::string> query
		= session.query<std::string>( "SELECT DISTINCT t.artist_name FROM track t " + sqlQuery.innerJoin().get() + " " + sqlQuery.where().get()).offset(offset).limit(size).orderBy("t.artist_name");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	typedef Wt::Dbo::collection< std::string > ArtistNames;
	ArtistNames artistNames(query);

	std::vector<std::string> res;
	for (ArtistNames::const_iterator it = artistNames.begin(); it != artistNames.end(); ++it)
		res.push_back((*it));

	return res;
}

std::vector<std::string>
Track::getReleases(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<std::string> query
		= session.query<std::string>( "SELECT DISTINCT t.release_name FROM track t " + sqlQuery.innerJoin().get() + " " + sqlQuery.where().get()).offset(offset).limit(size).orderBy("t.release_name");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	typedef Wt::Dbo::collection< std::string > ReleaseNames;
	ReleaseNames releaseNames(query);

	std::vector<std::string> res;
	for (ReleaseNames::const_iterator it = releaseNames.begin(); it != releaseNames.end(); ++it)
		res.push_back((*it));

	return res;
}

Genre::Genre()
{
}

Genre::Genre(const std::string& name)
: _name( std::string(name, 0, _maxNameLength) )
{
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

Wt::Dbo::collection<Genre::pointer>
Genre::getAll(Wt::Dbo::Session& session, int offset, int size)
{
	return session.find<Genre>().offset(offset).limit(size).orderBy("name");
}

Wt::Dbo::Query<Genre::GenreResult>
Genre::getAllQuery(Wt::Dbo::Session& session, SearchFilter& filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter, true);

	Wt::Dbo::Query<Genre::GenreResult> query
		= session.query<Genre::GenreResult>( "SELECT g.name, COUNT(DISTINCT t.id) FROM track t " + sqlQuery.innerJoin().get() + " " + sqlQuery.where().get()).groupBy("g.name").orderBy("g.name");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

void
Genre::updateGenreQueryModel(Wt::Dbo::Session& session,  Wt::Dbo::QueryModel<Genre::GenreResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query<Genre::GenreResult> query = getAllQuery(session, filter);
	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 2)
	{
		model.addColumn( "g.name", columnNames[0] );
		model.addColumn( "COUNT(DISTINCT t.id)", columnNames[1] );
	}
}

} // namespace Database

