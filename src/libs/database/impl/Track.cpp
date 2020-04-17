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

#include "database/Track.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/TrackFeatures.hpp"
#include "database/Session.hpp"
#include "utils/Logger.hpp"

#include "SqlQuery.hpp"

namespace Database {

Track::Track(const std::filesystem::path& p)
:
_filePath( p.string() )
{
}

std::vector<Track::pointer>
Track::getAll(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<Track::pointer> res {session.getDboSession().find<Track>()
		.limit(limit ? static_cast<int>(*limit) : -1)};

	return std::vector<Track::pointer>(std::cbegin(res), std::cend(res));
}

std::vector<Track::pointer>
Track::getAllRandom(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<Track::pointer> res {session.getDboSession().find<Track>()
		.limit(limit ? static_cast<int>(*limit) : -1)
		.orderBy("RANDOM()")};

	return std::vector<Track::pointer>(std::cbegin(res), std::cend(res));
}

std::vector<IdType>
Track::getAllIds(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<IdType> res = session.getDboSession().query<IdType>("SELECT id FROM track");
	return std::vector<IdType>(res.begin(), res.end());
}

Track::pointer
Track::getByPath(Session& session, const std::filesystem::path& p)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Track>().where("file_path = ?").bind(p.string());
}

Track::pointer
Track::getById(Session& session, IdType id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Track>()
		.where("id = ?").bind(id);
}

Track::pointer
Track::getByMBID(Session& session, const UUID& mbid)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Track>()
		.where("mbid = ?").bind(std::string {mbid.getAsString()});
}

Track::pointer
Track::create(Session& session, const std::filesystem::path& p)
{
	session.checkUniqueLocked();

	Track::pointer res {session.getDboSession().add(std::make_unique<Track>(p))};
	session.getDboSession().flush();

	return res;
}

std::vector<std::filesystem::path>
Track::getAllPaths(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<std::string> res = session.getDboSession().query<std::string>("SELECT file_path FROM track");
	return std::vector<std::filesystem::path>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getMBIDDuplicates(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> res = session.getDboSession().query<pointer>( "SELECT track FROM track WHERE mbid in (SELECT mbid FROM track WHERE mbid <> '' GROUP BY mbid HAVING COUNT (*) > 1)").orderBy("track.release_id,track.disc_number,track.track_number,track.mbid");
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getLastAdded(Session& session, const Wt::WDateTime& after, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<Track::pointer> res = session.getDboSession().find<Track>()
		.where("file_added > ?").bind(after)
		.orderBy("file_added DESC")
		.limit(limit ? static_cast<int>(*limit) : -1);

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getAllWithMBIDAndMissingFeatures(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> res = session.getDboSession().query<pointer>
		("SELECT t FROM track t")
		.where("LENGTH(t.mbid) > 0")
		.where("NOT EXISTS (SELECT * FROM track_features t_f WHERE t_f.track_id = t.id)");
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<IdType>
Track::getAllIdsWithFeatures(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<IdType> res = session.getDboSession().query<IdType>
		("SELECT t.id FROM track t")
		.where("EXISTS (SELECT * from track_features t_f WHERE t_f.track_id = t.id)")
		.limit(limit ? static_cast<int>(*limit) : -1);

	return std::vector<IdType>(res.begin(), res.end());
}

std::vector<IdType>
Track::getAllIdsWithClusters(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<IdType> res = session.getDboSession().query<IdType>
		("SELECT DISTINCT t.id FROM track t"
		 	" INNER JOIN track_cluster t_c ON t_c.track_id = t.id")
		.limit(limit ? static_cast<int>(*limit) : -1);

	return std::vector<IdType>(res.begin(), res.end());
}

std::vector<Cluster::pointer>
Track::getClusters() const
{
	std::vector< Cluster::pointer > clusters;
	std::copy(_clusters.begin(), _clusters.end(), std::back_inserter(clusters));
	return clusters;
}

std::vector<IdType>
Track::getClusterIds() const
{
	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	Wt::Dbo::collection<IdType> res = session()->query<IdType>
		("SELECT DISTINCT c.id FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id INNER JOIN track t ON t.id = t_c.track_id")
		.where("t.id = ?").bind(self()->id());

	return std::vector<IdType>(res.begin(), res.end());
}

bool
Track::hasTrackFeatures() const
{
	return (_trackFeatures.lock() != Database::TrackFeatures::pointer());
}

static
Wt::Dbo::Query< Track::pointer >
getQuery(Session& session,
		const std::set<IdType>& clusterIds,
		const std::vector<std::string>& keywords)
{
	session.checkSharedLocked();

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

	Wt::Dbo::Query<Track::pointer> query = session.getDboSession().query<Track::pointer>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
		query.bind(bindArg);

	return query;
}

std::vector<Track::pointer>
Track::getByFilter(Session& session,
		const std::set<IdType>& clusterIds,
		const std::vector<std::string>& keywords,
		std::optional<std::size_t> offset,
		std::optional<std::size_t> size,
		bool& moreResults)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> collection = getQuery(session, clusterIds, keywords)
		.limit(size ? static_cast<int>(*size) + 1 : -1)
		.offset(offset ? static_cast<int>(*offset) : -1);

	std::vector<pointer> res(collection.begin(), collection.end());

	if (size && (res.size() == static_cast<std::size_t>(*size) + 1))
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Track::pointer>
Track::getSimilarTracks(Session& session,
				const std::unordered_set<IdType>& tracks,
				std::optional<std::size_t> offset,
				std::optional<std::size_t> size)
{
	assert(!tracks.empty());
	session.checkSharedLocked();

	std::ostringstream oss;
	for (std::size_t i {}; i < tracks.size(); ++i)
	{
		if (!oss.str().empty())
			oss << ", ";
		oss << "?";
	}

	Wt::Dbo::Query<pointer> query {session.getDboSession().query<pointer>(
			"SELECT t FROM track t"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
					" AND t_c.cluster_id IN (SELECT c.id FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id WHERE t_c.track_id IN (" + oss.str() + "))"
					" AND t.id NOT IN (" + oss.str() + ")")
		.groupBy("t.id")
		.orderBy("COUNT(*) DESC")
		.limit(size ? static_cast<int>(*size) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1)};

	for (IdType trackId : tracks)
		query.bind(trackId );

	for (IdType trackId : tracks)
		query.bind(trackId );

	Wt::Dbo::collection<pointer> res = query;
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getByClusters(Session& session,
			const std::set<IdType>& clusters)
{
	assert(!clusters.empty());
	session.checkSharedLocked();

	bool moreResults;

	return getByFilter(session,
			clusters,
			{},
			{},
			{},
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

std::optional<std::size_t>
Track::getTrackNumber() const
{
	return (_trackNumber > 0) ? std::make_optional<std::size_t>(_trackNumber) : std::nullopt;
}

std::optional<std::size_t>
Track::getTotalTrack() const
{
	return (_totalTrack > 0) ? std::make_optional<std::size_t>(_totalTrack) : std::nullopt;
}

std::optional<std::size_t>
Track::getDiscNumber() const
{
	return (_discNumber > 0) ? std::make_optional<std::size_t>(_discNumber) : std::nullopt;
}

std::optional<std::size_t>
Track::getTotalDisc() const
{
	return (_totalDisc > 0) ? std::make_optional<std::size_t>(_totalDisc) : std::nullopt;
}

std::optional<int>
Track::getYear() const
{
	return (_year > 0) ? std::make_optional<int>(_year) : std::nullopt;
}

std::optional<int>
Track::getOriginalYear() const
{
	return (_originalYear > 0) ? std::make_optional<int>(_originalYear) : std::nullopt;
}

std::optional<std::string>
Track::getCopyright() const
{
	return _copyright != "" ? std::make_optional<std::string>(_copyright) : std::nullopt;
}

std::optional<std::string>
Track::getCopyrightURL() const
{
	return _copyrightURL != "" ? std::make_optional<std::string>(_copyrightURL) : std::nullopt;
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

std::vector<IdType>
Track::getArtistIds(TrackArtistLink::Type type) const
{
	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	Wt::Dbo::collection<IdType> artists {session()->query<IdType>("SELECT a.id from artist a INNER JOIN track_artist_link t_a_l ON a.id = t_a_l.artist_id INNER JOIN track t ON t.id = t_a_l.track_id")
		.where("t.id = ?").bind(self()->id())
		.where("t_a_l.type = ?").bind(type)};

	return std::vector<IdType>(artists.begin(), artists.end());
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

