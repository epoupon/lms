/*
 * Copyright (C) 2013-2016 Emeric Poupon
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

std::vector<Track::id_type>
Track::getAllIds(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<Track::id_type> res = session.query<Track::id_type>("SELECT id from track");
	return std::vector<Track::id_type>(res.begin(), res.end());
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

std::vector< Cluster::pointer >
Track::getClusters(void) const
{
	std::vector< Cluster::pointer > clusters;
	std::copy(_clusters.begin(), _clusters.end(), std::back_inserter(clusters));
	return clusters;
}

std::vector< Wt::Dbo::ptr<Feature> >
Track::getFeatures(void) const
{
	std::vector< Wt::Dbo::ptr<Feature> > features;
	std::copy(_features.begin(), _features.end(), std::back_inserter(features));
	return features;
}

Wt::Dbo::Query< Track::pointer >
Track::getQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<pointer> query
		= session.query<pointer>( "SELECT t FROM track t INNER JOIN artist a ON t.artist_id = a.id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN release r ON r.id = t.release_id " + sqlQuery.where().get()).groupBy("t.id").orderBy("a.name,t.date,r.name,t.disc_number,t.track_number");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Wt::Dbo::Query< Track::UIQueryResult >
Track::getUIQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<UIQueryResult> query
		= session.query<UIQueryResult>( "SELECT t.id, a.name, r.name, t.disc_number, t.track_number, t.name, t.duration, t.date, t.original_date, t.genre_list FROM track t INNER JOIN artist a ON t.artist_id = a.id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN release r ON r.id = t.release_id " + sqlQuery.where().get()).groupBy("t.id").orderBy("a.name,t.date,r.name,t.disc_number,t.track_number");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Track::StatsQueryResult
Track::getStats(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<StatsQueryResult> query = session.query<StatsQueryResult>( "SELECT COUNT(\"id\"), SUM(\"dur\") FROM  (SELECT t.id as \"id\", t.duration as \"dur\" FROM track t INNER JOIN artist a ON t.artist_id = a.id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN release r ON r.id = t.release_id " + sqlQuery.where().get() + " GROUP BY t.id)");

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

std::vector<Track::pointer>
Track::getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size, bool& moreResults)
{
	auto res = getByFilter(session, filter, offset, size + 1);

	if (size != -1 && res.size() == static_cast<std::size_t>(size) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
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


Cluster::Cluster()
{
}

Cluster::Cluster(std::string type, std::string name)
:
_type( std::string(type, 0, _maxTypeLength)),
_name( std::string(name, 0, _maxNameLength))
{
}

Wt::Dbo::collection<Cluster::pointer>
Cluster::getAll(Wt::Dbo::Session& session)
{
	return session.find<Cluster>();
}

Cluster::pointer
Cluster::get(Wt::Dbo::Session& session, std::string type, std::string name)
{
	// TODO use like search
	return session.find<Cluster>().where("type = ?").where("name = ?").bind( std::string(type, 0, _maxTypeLength)).bind( std::string(name, 0, _maxNameLength));
}

Cluster::pointer
Cluster::getNone(Wt::Dbo::Session& session)
{
	pointer res = get(session, "<None>", "<None>");
	if (!res)
		res = create(session, "<None>", "<None>");
	return res;
}

bool
Cluster::isNone(void) const
{
	return (_type == "<None>" && _name == "<None>");
}

Cluster::pointer
Cluster::create(Wt::Dbo::Session& session, std::string type, std::string name)
{
	return session.add(new Cluster(type, name));
}

void
Cluster::removeByType(Wt::Dbo::Session& session, std::string type)
{
	session.execute( "DELETE FROM cluster WHERE type = ?").bind(type);
}


Wt::Dbo::Query<Cluster::pointer>
Cluster::getQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<pointer> query
		= session.query<pointer>( "SELECT g FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id INNER JOIN artist a ON t.artist_id = a.id INNER JOIN release r ON r.id = t.release_id INNER JOIN track t ON t.id = t_c.track_id " + sqlQuery.where().get()).groupBy("c.name").orderBy("c.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

Wt::Dbo::Query<Cluster::UIQueryResult>
Cluster::getUIQuery(Wt::Dbo::Session& session, SearchFilter filter)
{
	SqlQuery sqlQuery = generatePartialQuery(filter);

	Wt::Dbo::Query<UIQueryResult> query
		= session.query<UIQueryResult>( "SELECT c.id, c.type, c.name, COUNT(DISTINCT t.id) FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id INNER JOIN artist a ON t.artist_id = a.id INNER JOIN release r ON r.id = t.release_id INNER JOIN track t ON t.id = t_c.track_id " + sqlQuery.where().get()).groupBy("c.name").orderBy("c.name");

	for (const std::string& bindArg : sqlQuery.where().getBindArgs())
		query.bind(bindArg);

	return query;
}

void
Cluster::updateUIQueryModel(Wt::Dbo::Session& session,  Wt::Dbo::QueryModel<UIQueryResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames)
{
	Wt::Dbo::Query<UIQueryResult> query = getUIQuery(session, filter);
	model.setQuery(query, columnNames.empty() ? true : false);

	// TODO do something better
	if (columnNames.size() == 3)
	{
		model.addColumn( "c.type", columnNames[0] );
		model.addColumn( "c.name", columnNames[1] );
		model.addColumn( "COUNT(DISTINCT t.id)", columnNames[2] );
	}
}

std::vector<Cluster::pointer>
Cluster::getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size)
{
	Wt::Dbo::collection<pointer> res = getQuery(session, filter).limit(size).offset(offset);

	return std::vector<pointer>(res.begin(), res.end());
}

Feature::Feature(Wt::Dbo::ptr<Track> track, const std::string& type, const std::string& value)
: _type(type),
_value(value),
_track(track)
{
}

Feature::pointer
Feature::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, const std::string& type, const std::string& value)
{
	return session.add(new Feature(track, type, value));
}

std::vector<Feature::pointer>
Feature::getByTrack(Wt::Dbo::Session& session, Track::id_type trackId, const std::string& type)
{
	Wt::Dbo::collection<pointer> res = session.find<Feature>().where("track_id = ? AND type = ?").bind( trackId).bind(type);
	return std::vector<pointer>(res.begin(), res.end());
}

} // namespace Database

