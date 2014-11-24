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

Track::Track(const boost::filesystem::path& p)
:
_trackNumber(0),
_discNumber(0),
_filePath( p.string() )
{
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

Wt::Dbo::collection< Track::pointer >
Track::getAll(Wt::Dbo::Session& session)
{
	return session.find<Track>();
}

std::vector< Genre::pointer >
Track::getGenres(void) const
{
	std::vector< Genre::pointer > genres;
	std::copy(_genres.begin(), _genres.end(), std::back_inserter(genres));
	return genres;
}

Wt::Dbo::collection< Track::pointer >
Track::getAll(Wt::Dbo::Session& session,
			const std::vector<std::string>& artists,
			const std::vector<std::string>& releases,
			const std::vector<std::string>& genres,
			int offset, int size)
{
	std::string sqlQuery = "SELECT t FROM track t";

	if (!genres.empty())
	{
		sqlQuery += " INNER JOIN genre g ON g.id = t_g.genre_id";
		sqlQuery += " INNER JOIN track_genre t_g ON t_g.track_id = t.id AND t_g.genre_id = g.id";
	}

	WhereClause where;

	{
		WhereClause artistWhere;

		for (std::size_t i = 0; i < artists.size(); ++i)
			artistWhere.Or( WhereClause("t.artist_name = ?") );

		where.And(artistWhere);
	}

	{
		WhereClause releaseWhere;

		for (std::size_t i = 0; i < releases.size(); ++i)
			releaseWhere.Or( WhereClause("t.release_name = ?") );

		where.And(releaseWhere);
	}

	{
		WhereClause genreWhere;

		for (std::size_t i = 0; i < genres.size(); ++i)
			genreWhere.Or( WhereClause("g.name = ?") );

		where.And(genreWhere);
	}


	Wt::Dbo::Query<Track::pointer> query = session.query<Track::pointer>( sqlQuery + " " + where.get() ).offset(offset).limit(size);

	BOOST_FOREACH(const std::string& artist, artists)
		query.bind(artist);

	BOOST_FOREACH(const std::string& release, releases)
		query.bind(release);

	BOOST_FOREACH(const std::string& genre, genres)
		query.bind(genre);

	query.groupBy("t");

	return query;
}

std::vector<std::string>
Track::getArtists(Wt::Dbo::Session& session,
		const std::vector<std::string>& genres,
		int offset, int size)
{
	std::string sqlQuery = "SELECT t.artist_name FROM track t";

	if (!genres.empty())
	{
		sqlQuery += " INNER JOIN genre g ON g.id = t_g.genre_id";
		sqlQuery += " INNER JOIN track_genre t_g ON t_g.track_id = t.id AND t_g.genre_id = g.id";
	}

	WhereClause whereClause;

	for (std::size_t i = 0; i < genres.size(); ++i)
		whereClause.Or( WhereClause("g.name = ?") );

	Wt::Dbo::Query<std::string> query = session.query<std::string>( sqlQuery + " " + whereClause.get() ).offset(offset).limit(size).groupBy("t.artist_name");

	BOOST_FOREACH(const std::string& genre, genres)
		query.bind(genre);

	typedef Wt::Dbo::collection< std::string > ArtistNames;
	ArtistNames artistNames(query);

	std::vector<std::string> res;
	for (ArtistNames::const_iterator it = artistNames.begin(); it != artistNames.end(); ++it)
		res.push_back((*it));

	return res;

}

std::vector<std::string>
Track::getReleases(Wt::Dbo::Session& session,
		const std::vector<std::string>& artists,
		const std::vector<std::string>& genres,
		int offset, int size)
{

	std::string sqlQuery = "SELECT t.release_name FROM track t";

	if (!genres.empty())
	{
		sqlQuery += " INNER JOIN genre g ON g.id = t_g.genre_id";
		sqlQuery += " INNER JOIN track_genre t_g ON t_g.track_id = t.id AND t_g.genre_id = g.id";
	}

	WhereClause whereClause;

	{
		WhereClause clause;

		BOOST_FOREACH(const std::string& artist, artists)
			clause.Or( WhereClause("t.artist_name = ?") ).bind(artist);

		whereClause.And(clause);
	}
	{
		WhereClause clause;

		BOOST_FOREACH(const std::string& genre, genres)
			clause.Or( WhereClause("g.name = ?") ).bind(genre);

		whereClause.And(clause);
	}

	Wt::Dbo::Query<std::string> query = session.query<std::string>( sqlQuery + " " + whereClause.get() ).offset(offset).limit(size).groupBy("t.release_name");

	BOOST_FOREACH(const std::string& bindArg, whereClause.getBindArgs())
		query.bind(bindArg);

	typedef Wt::Dbo::collection< std::string > ReleaseNames;
	ReleaseNames releaseNames(query);

	std::vector<std::string> res;
	for (ReleaseNames::const_iterator it = releaseNames.begin(); it != releaseNames.end(); ++it)
		res.push_back((*it));

	return res;
}


} // namespace Database



