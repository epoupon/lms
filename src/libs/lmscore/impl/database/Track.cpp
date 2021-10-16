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

#include "lmscore/database/Track.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "lmscore/database/Artist.hpp"
#include "lmscore/database/Cluster.hpp"
#include "lmscore/database/Release.hpp"
#include "lmscore/database/TrackArtistLink.hpp"
#include "lmscore/database/TrackFeatures.hpp"
#include "lmscore/database/Session.hpp"
#include "lmscore/database/User.hpp"
#include "utils/Logger.hpp"

#include "SqlQuery.hpp"
#include "StringViewTraits.hpp"
#include "Traits.hpp"
#include "Utils.hpp"

namespace Database {

template <typename T>
static
Wt::Dbo::Query<T>
createQuery(Session& session,
		const std::string& queryStr,
		const std::vector<ClusterId>& clusterIds,
		const std::vector<std::string_view>& keywords)
{
	session.checkSharedLocked();

	auto query {session.getDboSession().query<T>(queryStr)};

	for (std::string_view keyword : keywords)
		query.where("t.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + escapeLikeKeyword(keyword) + "%");

	if (!clusterIds.empty())
	{
		std::ostringstream oss;
		oss << "t.id IN (SELECT DISTINCT t.id FROM track t"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
			" INNER JOIN cluster c ON c.id = t_c.cluster_id";

		WhereClause clusterClause;
		for (const ClusterId clusterId : clusterIds)
		{
			clusterClause.Or(WhereClause("c.id = ?"));
			query.bind(clusterId);
		}

		oss << " " << clusterClause.get();
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size() << ")";

		query.where(oss.str());
	}

	return query;
}

Track::Track(const std::filesystem::path& p)
: _filePath {p.string()}
{
}

std::size_t
Track::getCount(Session& session)
{
	session.checkSharedLocked();

	return session.getDboSession().query<int>("SELECT COUNT(*) FROM track");
}

std::vector<Track::pointer>
Track::getAll(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<Track>()
		.limit(limit ? static_cast<int>(*limit) : -1)
		.resultList()};

	return std::vector<Track::pointer>(std::cbegin(res), std::cend(res));
}

std::vector<Track::pointer>
Track::getAllRandom(Session& session, const std::vector<ClusterId>& clusterIds, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	auto query {createQuery<Wt::Dbo::ptr<Track>>(session, "SELECT t from track t", clusterIds, {})};
	auto collection {query
		.orderBy("RANDOM()")
		.limit(limit ? static_cast<int>(*limit) + 1: -1)
		.resultList()};

	return std::vector<pointer>(collection.begin(), collection.end());
}

std::vector<TrackId>
Track::getAllIdsRandom(Session& session, const std::vector<ClusterId>& clusterIds, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	auto query {createQuery<TrackId>(session, "SELECT t.id from track t", clusterIds, {})};

	Wt::Dbo::collection<TrackId> collection = query
		.orderBy("RANDOM()")
		.limit(limit ? static_cast<int>(*limit) + 1: -1);

	return std::vector<TrackId>(collection.begin(), collection.end());
}


std::vector<TrackId>
Track::getAllIds(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<TrackId> res = session.getDboSession().query<TrackId>("SELECT id FROM track");
	return std::vector<TrackId>(res.begin(), res.end());
}

Track::pointer
Track::getByPath(Session& session, const std::filesystem::path& p)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Track>().where("file_path = ?").bind(p.string()).resultValue();
}

Track::pointer
Track::getById(Session& session, TrackId id)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Track>()
		.where("id = ?").bind(id)
		.resultValue();
}

bool
Track::exists(Session& session, TrackId id)
{
	session.checkSharedLocked();

	return session.getDboSession().query<int>("SELECT 1 from track").where("id = ?").bind(id).resultValue() == 1;
}

std::vector<Track::pointer>
Track::getByRecordingMBID(Session& session, const UUID& mbid)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<Track>()
		.where("recording_mbid = ?").bind(std::string {mbid.getAsString()})
		.resultList()};

	return std::vector<Track::pointer>(res.begin(), res.end());
}

Track::pointer
Track::create(Session& session, const std::filesystem::path& p)
{
	session.checkUniqueLocked();

	Track::pointer res {session.getDboSession().add(std::make_unique<Track>(p))};
	session.getDboSession().flush();

	return res;
}

std::vector<std::pair<TrackId, std::filesystem::path>>
Track::getAllPaths(Session& session, std::optional<std::size_t> offset, std::optional<std::size_t> size)
{
	using QueryResultType = std::tuple<TrackId, std::string>;
	session.checkSharedLocked();

	Wt::Dbo::collection<QueryResultType> queryRes = session.getDboSession().query<QueryResultType>("SELECT id,file_path FROM track")
		.limit(size ? static_cast<int>(*size) + 1 : -1)
		.offset(offset ? static_cast<int>(*offset) : -1);

	std::vector<std::pair<TrackId, std::filesystem::path>> result;
	result.reserve(queryRes.size());

	std::transform(std::begin(queryRes), std::end(queryRes), std::back_inserter(result),
			[](const QueryResultType& queryResult)
			{
				return std::make_pair(std::get<0>(queryResult), std::get<1>(queryResult));
			});

	return result;
}

std::vector<Track::pointer>
Track::getMBIDDuplicates(Session& session)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().query<Wt::Dbo::ptr<Track>>( "SELECT track FROM track WHERE mbid in (SELECT mbid FROM track WHERE mbid <> '' GROUP BY mbid HAVING COUNT (*) > 1)")
		.orderBy("track.release_id,track.disc_number,track.track_number,track.mbid")
		.resultList()};
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getLastWritten(Session& session, std::optional<Wt::WDateTime> after, const std::vector<ClusterId>& clusterIds, std::optional<Range> range, bool& moreResults)
{
	session.checkSharedLocked();

	auto query {createQuery<Wt::Dbo::ptr<Track>>(session, "SELECT t from track t", clusterIds, {})};
	if (after)
		query.where("t.file_last_write > ?").bind(after);

	auto collection {query
		.orderBy("t.file_last_write DESC")
		.groupBy("t.id")
		.offset(range ? static_cast<int>(range->offset) : -1)
		.limit(range ? static_cast<int>(range->limit) + 1: -1)
		.resultList()};

	std::vector<pointer> res(collection.begin(), collection.end());
	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Track::pointer>
Track::getAllWithRecordingMBIDAndMissingFeatures(Session& session)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().query<Wt::Dbo::ptr<Track>>
		("SELECT t FROM track t")
		.where("LENGTH(t.recording_mbid) > 0")
		.where("NOT EXISTS (SELECT * FROM track_features t_f WHERE t_f.track_id = t.id)")
		.resultList()};
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<TrackId>
Track::getAllIdsWithFeatures(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<TrackId> res = session.getDboSession().query<TrackId>
		("SELECT t.id FROM track t")
		.where("EXISTS (SELECT * from track_features t_f WHERE t_f.track_id = t.id)")
		.limit(limit ? static_cast<int>(*limit) : -1);

	return std::vector<TrackId>(res.begin(), res.end());
}

std::vector<TrackId>
Track::getAllIdsWithClusters(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<TrackId> res = session.getDboSession().query<TrackId>
		("SELECT DISTINCT t.id FROM track t"
		 	" INNER JOIN track_cluster t_c ON t_c.track_id = t.id")
		.limit(limit ? static_cast<int>(*limit) : -1);

	return std::vector<TrackId>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getStarred(Session& session,
		ObjectPtr<User> user,
		const std::vector<ClusterId>& clusterIds,
		std::optional<Range> range, bool& moreResults)
{
	session.checkSharedLocked();

	auto query {createQuery<Wt::Dbo::ptr<Track>>(session, "SELECT t from track t", clusterIds, {})};
	{
		std::ostringstream oss;
		oss << "t.id IN (SELECT DISTINCT t.id FROM track t"
			" INNER JOIN user_track_starred uts ON uts.track_id = t.id"
			" INNER JOIN user u ON u.id = uts.user_id WHERE u.id = ?)";

		query.bind(user->getId().toString());
		query.where(oss.str());
	}

	auto collection {query
		.offset(range ? static_cast<int>(range->offset) : -1)
		.limit(range ? static_cast<int>(range->limit) + 1: -1)
		.resultList()};

	std::vector<pointer> res(collection.begin(), collection.end());
	if (range && res.size() == static_cast<std::size_t>(range->limit) + 1)
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Cluster::pointer>
Track::getClusters() const
{
	return std::vector<Cluster::pointer>(_clusters.begin(), _clusters.end());
}

std::vector<ClusterId>
Track::getClusterIds() const
{
	assert(session());

	auto res {session()->query<ClusterId>
		("SELECT DISTINCT c.id FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id INNER JOIN track t ON t.id = t_c.track_id")
		.where("t.id = ?").bind(getId())
		.resultList()};

	return std::vector<ClusterId>(res.begin(), res.end());
}

bool
Track::hasTrackFeatures() const
{
	return (_trackFeatures.lock() != Wt::Dbo::ptr<Database::TrackFeatures> {});
}

std::vector<Track::pointer>
Track::getByFilter(Session& session,
		const std::vector<ClusterId>& clusterIds,
		const std::vector<std::string_view>& keywords,
		std::optional<Range> range,
		bool& moreResults)
{
	session.checkSharedLocked();

	auto collection {createQuery<Wt::Dbo::ptr<Track>>(session, "SELECT t from track t", clusterIds, keywords)
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1)
		.resultList()};

	std::vector<pointer> res(collection.begin(), collection.end());
	if (range && (res.size() == static_cast<std::size_t>(range->limit) + 1))
	{
		moreResults = true;
		res.pop_back();
	}
	else
		moreResults = false;

	return res;
}

std::vector<Track::pointer>
Track::getByNameAndReleaseName(Session& session, std::string_view trackName, std::string_view releaseName)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().query<Wt::Dbo::ptr<Track>>("SELECT t from track t")
		.join("release r ON t.release_id = r.id")
		.where("t.name = ?").bind(trackName)
		.where("r.name = ?").bind(releaseName)
		.resultList()};
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getSimilarTracks(Session& session,
				const std::vector<TrackId>& tracks,
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

	auto query {session.getDboSession().query<Wt::Dbo::ptr<Track>>(
			"SELECT t FROM track t"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
					" AND t_c.cluster_id IN (SELECT c.id FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id WHERE t_c.track_id IN (" + oss.str() + "))"
					" AND t.id NOT IN (" + oss.str() + ")")
		.groupBy("t.id")
		.orderBy("COUNT(*) DESC, RANDOM()")
		.limit(size ? static_cast<int>(*size) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1)};

	for (TrackId trackId : tracks)
		query.bind(trackId);

	for (TrackId trackId : tracks)
		query.bind(trackId);

	auto res {query.resultList()};
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::getByClusters(Session& session, const std::vector<ClusterId>& clusters)
{
	assert(!clusters.empty());
	session.checkSharedLocked();

	bool moreResults;
	return getByFilter(session,
			clusters,
			{}, // keywords
			std::nullopt, // range
			moreResults);
}

void
Track::clearArtistLinks()
{
	_trackArtistLinks.clear();
}

void
Track::addArtistLink(const ObjectPtr<TrackArtistLink>& artistLink)
{
	_trackArtistLinks.insert(getDboPtr(artistLink));
}

void
Track::setClusters(const std::vector<ObjectPtr<Cluster>>& clusters)
{
	_clusters.clear();
	for (const ObjectPtr<Cluster>& cluster : clusters)
		_clusters.insert(getDboPtr(cluster));
}

void
Track::setFeatures(const ObjectPtr<TrackFeatures>& features)
{
	_trackFeatures = getDboPtr(features);
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
	return (_date.isValid() ? std::make_optional<int>(_date.year()) : std::nullopt);
}

std::optional<int>
Track::getOriginalYear() const
{
	return (_originalDate.isValid() ? std::make_optional<int>(_originalDate.year()) : std::nullopt);
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

std::vector<Artist::pointer>
Track::getArtists(EnumSet<TrackArtistLinkType> linkTypes) const
{
	assert(session());

	std::ostringstream oss;
	oss <<
		"SELECT a from artist a"
		" INNER JOIN track_artist_link t_a_l ON a.id = t_a_l.artist_id"
		" INNER JOIN track t ON t.id = t_a_l.track_id";

	if (!linkTypes.empty())
	{
		oss << " AND t_a_l.type IN (";

		bool first {true};
		for (TrackArtistLinkType type : linkTypes)
		{
			(void) type;
			if (!first)
				oss << ", ";
			oss << "?";
			first = false;
		}
		oss << ")";
	}

	auto query {session()->query<Wt::Dbo::ptr<Artist>>(oss.str())};
	for (TrackArtistLinkType type : linkTypes)
		query.bind(type);

	query.where("t.id = ?").bind(getId());

	auto res {query.resultList()};
	return std::vector<Artist::pointer>(std::begin(res), std::end(res));
}

std::vector<ArtistId>
Track::getArtistIds(EnumSet<TrackArtistLinkType> linkTypes) const
{
	assert(self());
	assert(session());

	std::ostringstream oss;
	oss <<
		"SELECT a.id from artist a"
		" INNER JOIN track_artist_link t_a_l ON a.id = t_a_l.artist_id"
		" INNER JOIN track t ON t.id = t_a_l.track_id";

	if (!linkTypes.empty())
	{
		oss << " AND t_a_l.type IN (";

		bool first {true};
		for (TrackArtistLinkType type : linkTypes)
		{
			(void) type;
			if (!first)
				oss << ", ";
			oss << "?";
			first = false;
		}
		oss << ")";
	}

	Wt::Dbo::Query<ArtistId> query {session()->query<ArtistId>(oss.str())
		.where("t.id = ?").bind(getId())};

	for (TrackArtistLinkType type : linkTypes)
		query.bind(type);

	Wt::Dbo::collection<ArtistId> res = query;
	return std::vector<ArtistId>(std::begin(res), std::end(res));
}

std::vector<TrackArtistLink::pointer>
Track::getArtistLinks() const
{
	return std::vector<TrackArtistLink::pointer>(_trackArtistLinks.begin(), _trackArtistLinks.end());
}

ObjectPtr<TrackFeatures>
Track::getTrackFeatures() const
{
	return _trackFeatures.lock();
}

std::vector<std::vector<Cluster::pointer>>
Track::getClusterGroups(const std::vector<ClusterType::pointer>& clusterTypes, std::size_t size) const
{
	assert(self());
	assert(session());

	WhereClause where;

	std::ostringstream oss;

	oss << "SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN cluster_type c_type ON c.cluster_type_id = c_type.id";

	where.And(WhereClause("t.id = ?")).bind(getId().toString());
	{
		WhereClause clusterClause;
		for (auto clusterType : clusterTypes)
			clusterClause.Or(WhereClause("c_type.id = ?")).bind(clusterType->getId().toString());
		where.And(clusterClause);
	}
	oss << " " << where.get();
	oss << " GROUP BY c.id ORDER BY COUNT(c.id) DESC";

	auto query {session()->query<Wt::Dbo::ptr<Cluster>>(oss.str())};
	for (const std::string& bindArg : where.getBindArgs())
		query.bind(bindArg);

	auto queryRes {query.resultList()};

	std::map<ClusterTypeId, std::vector<Cluster::pointer>> clusters;
	for (const Wt::Dbo::ptr<Cluster>& cluster : queryRes)
	{
		if (clusters[cluster->getType()->getId()].size() < size)
			clusters[cluster->getType()->getId()].push_back(cluster);
	}

	std::vector<std::vector<Cluster::pointer>> res;
	for (auto cluster_list : clusters)
		res.push_back(cluster_list.second);

	return res;
}

} // namespace Database

