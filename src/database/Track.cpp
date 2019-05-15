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

#include "Track.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "utils/Logger.hpp"

#include "Artist.hpp"
#include "Cluster.hpp"
#include "Release.hpp"
#include "TrackFeatures.hpp"
#include "SqlQuery.hpp"

namespace Database {

Track::Track(const boost::filesystem::path& p)
:
_filePath( p.string() )
{
}

std::vector<Track::pointer>
Track::getAll(Wt::Dbo::Session& session, boost::optional<std::size_t> limit)
{
	Wt::Dbo::collection<Track::pointer> res {session.find<Track>()
		.limit(limit ? static_cast<int>(*limit) : -1)};

	return std::vector<Track::pointer>(std::cbegin(res), std::cend(res));
}

std::vector<Track::pointer>
Track::getAllRandom(Wt::Dbo::Session& session, boost::optional<std::size_t> limit)
{
	Wt::Dbo::collection<Track::pointer> res {session.find<Track>()
		.limit(limit ? static_cast<int>(*limit) : -1)
		.orderBy("RANDOM()")};

	return std::vector<Track::pointer>(std::cbegin(res), std::cend(res));
}

std::vector<IdType>
Track::getAllIds(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction(session);
	Wt::Dbo::collection<IdType> res = session.query<IdType>("SELECT id from track");
	return std::vector<IdType>(res.begin(), res.end());
}

Track::pointer
Track::getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.find<Track>().where("file_path = ?").bind(p.string());
}

Track::pointer
Track::getById(Wt::Dbo::Session& session, IdType id)
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
	return session.add(std::make_unique<Track>(p));
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

std::vector<Track::pointer>
Track::getLastAdded(Wt::Dbo::Session& session, Wt::WDateTime after, int limit)
{
	Wt::Dbo::collection<Track::pointer> res = session.find<Track>()
		.where("file_added > ?").bind(after)
		.orderBy("file_added DESC")
		.limit(limit);

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getAllWithMBIDAndMissingFeatures(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.query<pointer>
		("SELECT t FROM track t")
		.where("LENGTH(t.mbid) > 0")
		.where("NOT EXISTS (SELECT * FROM track_features t_f WHERE t_f.track_id = t.id)");
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<IdType>
Track::getAllIdsWithFeatures(Wt::Dbo::Session& session, boost::optional<std::size_t> limit)
{
	int size {limit ? static_cast<int>(*limit) : -1};

	Wt::Dbo::collection<IdType> res = session.query<IdType>
		("SELECT t.id FROM track t")
		.where("EXISTS (SELECT * from track_features t_f WHERE t_f.track_id = t.id)")
		.limit(size);

	return std::vector<IdType>(res.begin(), res.end());
}

std::vector<Cluster::pointer>
Track::getClusters(void) const
{
	std::vector< Cluster::pointer > clusters;
	std::copy(_clusters.begin(), _clusters.end(), std::back_inserter(clusters));
	return clusters;
}

bool
Track::hasTrackFeatures() const
{
	return (_trackFeatures.lock() != Database::TrackFeatures::pointer());
}

static
Wt::Dbo::Query< Track::pointer >
getQuery(Wt::Dbo::Session& session,
		const std::set<IdType>& clusterIds,
		const std::vector<std::string> keywords)
{
	WhereClause where;

	std::ostringstream oss;
	oss << "SELECT t FROM track t";

	for (auto keyword : keywords)
		where.And(WhereClause("t.name LIKE ?")).bind("%%" + keyword + "%%");

	if (!clusterIds.empty())
	{
		oss << " INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;

		for (auto id : clusterIds)
			clusterClause.Or(WhereClause("c.id = ?")).bind(std::to_string(id));

		where.And(clusterClause);
	}

	oss << " " << where.get();

	if (!clusterIds.empty())
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size();

	oss << " ORDER BY t.name COLLATE NOCASE";

	Wt::Dbo::Query<Track::pointer> query = session.query<Track::pointer>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
		query.bind(bindArg);

	return query;
}

std::vector<Track::pointer>
Track::getByFilter(Wt::Dbo::Session& session,
		const std::set<IdType>& clusterIds,
		const std::vector<std::string> keywords,
		boost::optional<std::size_t> offset,
		boost::optional<std::size_t> size,
		bool& moreResults)
{
	Wt::Dbo::collection<pointer> collection = getQuery(session, clusterIds, keywords)
		.limit(size ? static_cast<int>(*size) + 1 : -1)
		.offset(offset ? static_cast<int>(*offset) : -1);

	auto res = std::vector<pointer>(collection.begin(), collection.end());

	if (size && res.size() == static_cast<std::size_t>(*size) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Track::pointer>
Track::getByFilter(Wt::Dbo::Session& session,
			const std::set<IdType>& clusters)
{
	bool moreResults;

	return getByFilter(session,
			clusters,
			std::vector<std::string> {},
			boost::optional<std::size_t> {},
			boost::optional<std::size_t> {},
			moreResults);
}

void
Track::clearArtistLinks()
{
	_trackArtistLinks.clear();
}

void
Track::addArtistLink(const Wt::Dbo::ptr<TrackArtistLink>& artistLink)
{
	_trackArtistLinks.insert(artistLink);
}

void
Track::setClusters(const std::vector<Wt::Dbo::ptr<Cluster>>& clusters)
{
	_clusters.clear();
	for (const Wt::Dbo::ptr<Cluster>& cluster : clusters)
		_clusters.insert(cluster);
}

void
Track::setFeatures(const Wt::Dbo::ptr<TrackFeatures>& features)
{
	_trackFeatures = features;
}

boost::optional<std::size_t>
Track::getTrackNumber(void) const
{
	return (_trackNumber > 0) ? boost::make_optional<std::size_t>(_trackNumber) : boost::none;
}

boost::optional<std::size_t>
Track::getDiscNumber(void) const
{
	return (_discNumber > 0) ? boost::make_optional<std::size_t>(_discNumber) : boost::none;
}

boost::optional<int>
Track::getYear() const
{
	return (_year > 0) ? boost::make_optional<int>(_year) : boost::none;
}

boost::optional<int>
Track::getOriginalYear() const
{
	return (_originalYear > 0) ? boost::make_optional<int>(_originalYear) : boost::none;
}

boost::optional<std::string>
Track::getCopyright() const
{
	return _copyright != "" ? boost::make_optional<std::string>(_copyright) : boost::none;
}

boost::optional<std::string>
Track::getCopyrightURL() const
{
	return _copyrightURL != "" ? boost::make_optional<std::string>(_copyrightURL) : boost::none;
}

std::vector<Wt::Dbo::ptr<Artist>>
Track::getArtists(TrackArtistLink::Type type) const
{
	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	Wt::Dbo::collection<Wt::Dbo::ptr<Artist>> artists {session()->query<Artist::pointer>("SELECT a from artist a INNER JOIN track_artist_link t_a_l ON a.id = t_a_l.artist_id INNER JOIN track t ON t.id = t_a_l.track_id")
		.where("t.id = ?").bind(self()->id())
		.where("t_a_l.type = ?").bind(type)};

	return std::vector<Wt::Dbo::ptr<Artist>>(artists.begin(), artists.end());
}

std::vector<Wt::Dbo::ptr<TrackArtistLink>>
Track::getArtistLinks() const
{
	return std::vector<Wt::Dbo::ptr<TrackArtistLink>>(_trackArtistLinks.begin(), _trackArtistLinks.end());
}

Wt::Dbo::ptr<TrackFeatures>
Track::getTrackFeatures() const
{
	return _trackFeatures.lock();
}

std::vector<std::vector<Cluster::pointer>>
Track::getClusterGroups(std::vector<ClusterType::pointer> clusterTypes, std::size_t size) const
{
	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	WhereClause where;

	std::ostringstream oss;

	oss << "SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN cluster_type c_type ON c.cluster_type_id = c_type.id";

	where.And(WhereClause("t.id = ?")).bind(std::to_string(self()->id()));
	{
		WhereClause clusterClause;
		for (auto clusterType : clusterTypes)
			clusterClause.Or(WhereClause("c_type.id = ?")).bind(std::to_string(clusterType.id()));
		where.And(clusterClause);
	}
	oss << " " << where.get();
	oss << " GROUP BY c.id ORDER BY COUNT(c.id) DESC";

	Wt::Dbo::Query<Cluster::pointer> query = session()->query<Cluster::pointer>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
		query.bind(bindArg);

	Wt::Dbo::collection<Cluster::pointer> queryRes = query;

	std::map<IdType, std::vector<Cluster::pointer>> clusters;
	for (auto cluster : queryRes)
	{
		if (clusters[cluster->getType().id()].size() < size)
			clusters[cluster->getType().id()].push_back(cluster);
	}

	std::vector<std::vector<Cluster::pointer>> res;
	for (auto cluster_list : clusters)
		res.push_back(cluster_list.second);

	return res;
}

} // namespace Database

