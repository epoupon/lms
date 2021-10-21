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

#include "services/database/Release.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/Logger.hpp"
#include "SqlQuery.hpp"
#include "Traits.hpp"
#include "Utils.hpp"

namespace Database
{

template <typename T>
static
Wt::Dbo::Query<T>
createQuery(Session& session,
			const std::string& queryStr,
			const std::vector<ClusterId>& clusterIds,
			const std::vector<std::string_view>& keywords)
{

	auto query {session.getDboSession().query<T>(queryStr)};
	query.join("track t ON t.release_id = r.id");

	for (std::string_view keyword : keywords)
		query.where("r.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + escapeLikeKeyword(keyword) + "%");

	if (!clusterIds.empty())
	{
		std::ostringstream oss;
		oss << "r.id IN (SELECT DISTINCT r.id FROM release r"
			" INNER JOIN track t ON t.release_id = r.id"
			" INNER JOIN cluster c ON c.id = t_c.cluster_id"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

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

Release::Release(const std::string& name, const std::optional<UUID>& MBID)
: _name {std::string(name, 0 , _maxNameLength)},
_MBID {MBID ? MBID->getAsString() : ""}
{
}

std::vector<Release::pointer>
Release::getByName(Session& session, const std::string& name)
{
	session.checkUniqueLocked();

	auto res {session.getDboSession()
						.find<Release>()
						.where("name = ?").bind( std::string(name, 0, _maxNameLength) )
						.resultList()};

	return std::vector<Release::pointer>(res.begin(), res.end());
}

Release::pointer
Release::getByMBID(Session& session, const UUID& mbid)
{
	session.checkSharedLocked();

	return session.getDboSession()
					.find<Release>()
					.where("mbid = ?").bind(std::string {mbid.getAsString()})
					.resultValue();;
}

Release::pointer
Release::getById(Session& session, ReleaseId id)
{
	session.checkSharedLocked();

	return session.getDboSession()
					.find<Release>()
					.where("id = ?").bind(id)
					.resultValue();
}

bool
Release::exists(Session& session, ReleaseId id)
{
	session.checkSharedLocked();
	return session.getDboSession().query<int>("SELECT 1 FROM release").where("id = ?").bind(id).resultValue() == 1;
}

Release::pointer
Release::create(Session& session, const std::string& name, const std::optional<UUID>& MBID)
{
	session.checkSharedLocked();

	Release::pointer res {session.getDboSession().add(std::make_unique<Release>(name, MBID))};
	session.getDboSession().flush();

	return res;
}

std::size_t
Release::getCount(Session& session)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Release>().resultList().size();
}

std::vector<Release::pointer>
Release::getAll(Session& session, std::optional<Range> range)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<Release>()
		.offset(range ? static_cast<int>(range->offset) : -1)
		.limit(range ? static_cast<int>(range->limit) : -1)
		.orderBy("name COLLATE NOCASE")
		.resultList()};

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<ReleaseId>
Release::getAllIds(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<ReleaseId> res = session.getDboSession().query<ReleaseId>("SELECT id FROM release");
	return std::vector<ReleaseId>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getAllOrderedByArtist(Session& session, std::optional<std::size_t> offset, std::optional<std::size_t> size)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().query<Wt::Dbo::ptr<Release>>(
			"SELECT DISTINCT r FROM release r"
			" INNER JOIN track t ON r.id = t.release_id"
			" INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id"
			" INNER JOIN artist a ON t_a_l.artist_id = a.id")
		 .offset(offset ? static_cast<int>(*offset) : -1)
		 .limit(size ? static_cast<int>(*size) : -1)
		 .orderBy("a.name COLLATE NOCASE, r.name COLLATE NOCASE")
		 .resultList()};

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getAllRandom(Session& session, const std::vector<ClusterId>& clusterIds, std::optional<std::size_t> size)
{
	session.checkSharedLocked();

	auto query {createQuery<Wt::Dbo::ptr<Release>>(session, "SELECT DISTINCT r from release r", clusterIds, {})};
	auto res {query
		.orderBy("RANDOM()")
		.limit(size ? static_cast<int>(*size) : -1)
		.resultList()};

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<ReleaseId>
Release::getAllIdsRandom(Session& session, const std::vector<ClusterId>& clusterIds, std::optional<std::size_t> size)
{
	session.checkSharedLocked();

	auto query {createQuery<ReleaseId>(session, "SELECT DISTINCT r.id from release r", clusterIds, {})};

	Wt::Dbo::collection<ReleaseId> res = query
		.orderBy("RANDOM()")
		.limit(size ? static_cast<int>(*size) : -1);

	return std::vector<ReleaseId>(res.begin(), res.end());
}


std::vector<Release::pointer>
Release::getAllOrphans(Session& session)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().query<Wt::Dbo::ptr<Release>>("select r from release r LEFT OUTER JOIN Track t ON r.id = t.release_id WHERE t.id IS NULL").resultList()};
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getLastWritten(Session& session,
		std::optional<Wt::WDateTime> after,
		const std::vector<ClusterId>& clusterIds,
		std::optional<Range> range,
		bool& moreResults)
{
	session.checkSharedLocked();

	auto query {createQuery<Wt::Dbo::ptr<Release>>(session, "SELECT r from release r", clusterIds, {})};
	if (after)
		query.where("t.file_last_write > ?").bind(after);

	auto collection {query
		.orderBy("t.file_last_write DESC")
		.groupBy("r.id")
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

std::vector<Release::pointer>
Release::getByYear(Session& session, int yearFrom, int yearTo, std::optional<Range> range)
{
	auto res {session.getDboSession().query<Wt::Dbo::ptr<Release>>
		("SELECT DISTINCT r from release r INNER JOIN track t ON r.id = t.release_id")
		.where("t.date >= ?").bind(Wt::WDate {yearFrom, 1, 1})
		.where("t.date <= ?").bind(Wt::WDate {yearTo, 12, 31})
		.orderBy("t.date, r.name COLLATE NOCASE")
		.offset(range ? static_cast<int>(range->offset) : -1)
		.limit(range ? static_cast<int>(range->limit) : -1)
		.resultList()};

	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getStarred(Session& session,
		User::pointer user,
		const std::vector<ClusterId>& clusterIds,
		std::optional<Range> range,
		bool& moreResults)
{
	session.checkSharedLocked();

	auto query {createQuery<Wt::Dbo::ptr<Release>>(session, "SELECT r from release r", clusterIds, {})};
	{
		std::ostringstream oss;
		oss << "r.id IN (SELECT DISTINCT r.id FROM release r"
			" INNER JOIN user_release_starred urs ON urs.release_id = r.id"
			" INNER JOIN user u ON u.id = urs.user_id WHERE u.id = ?)";

		query.bind(user->getId());
		query.where(oss.str());
	}

	auto collection {query
		.groupBy("r.id")
		.orderBy("r.name COLLATE NOCASE")
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

std::vector<Release::pointer>
Release::getByClusters(Session& session, const std::vector<ClusterId>& clusters)
{
	assert(!clusters.empty());

	session.checkSharedLocked();

	bool moreResults;
	return getByFilter(session, clusters, {}, std::nullopt, moreResults);
}

std::vector<Release::pointer>
Release::getByFilter(Session& session,
		const std::vector<ClusterId>& clusterIds,
		const std::vector<std::string_view>& keywords,
		std::optional<Range> range,
		bool& moreResults)
{
	session.checkSharedLocked();

	auto collection {createQuery<Wt::Dbo::ptr<Release>>(session, "SELECT r from release r", clusterIds, keywords)
		.groupBy("r.id")
		.orderBy("r.name COLLATE NOCASE")
		.limit(range ? static_cast<int>(range->limit) + 1 : -1)
		.offset(range ? static_cast<int>(range->offset) : -1)
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

std::vector<ReleaseId>
Release::getAllIdsWithClusters(Session& session, std::optional<std::size_t> limit)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<ReleaseId> res = session.getDboSession().query<ReleaseId>
		("SELECT DISTINCT r.id FROM release r"
		 	" INNER JOIN track t ON t.release_id = r.id"
		 	" INNER JOIN track_cluster t_c ON t_c.track_id = t.id")
		.limit(limit ? static_cast<int>(*limit) : -1);

	return std::vector<ReleaseId>(res.begin(), res.end());
}


std::optional<std::size_t>
Release::getTotalTrack(void) const
{
	assert(session());

	int res = session()->query<int>("SELECT COALESCE(MAX(total_track),0) FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.bind(getId());

	return (res > 0) ? std::make_optional<std::size_t>(res) : std::nullopt;
}

std::optional<std::size_t>
Release::getTotalDisc(void) const
{
	assert(session());

	int res = session()->query<int>("SELECT COALESCE(MAX(total_disc),0) FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.bind(getId());

	return (res > 0) ? std::make_optional<std::size_t>(res) : std::nullopt;
}

std::optional<int>
Release::getReleaseYear(bool original) const
{
	assert(session());

	const char* field {original ? "original_date" : "date"};

	auto dates {session()->query<Wt::WDate>(
			std::string {"SELECT "} + "t." + field + " FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.groupBy(field)
		.bind(getId())
		.resultList()};

	// various dates => no date
	if (dates.empty() || dates.size() > 1)
		return std::nullopt;

	auto date {dates.front().year()};

	if (date > 0)
		return date;

	return std::nullopt;
}

std::optional<std::string>
Release::getCopyright() const
{
	assert(session());

	Wt::Dbo::collection<std::string> copyrights = session()->query<std::string>
		("SELECT copyright FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.groupBy("copyright")
		.bind(getId());

	std::vector<std::string> values(copyrights.begin(), copyrights.end());

	// various copyrights => no copyright
	if (values.empty() || values.size() > 1 || values.front().empty())
		return std::nullopt;

	return values.front();
}

std::optional<std::string>
Release::getCopyrightURL() const
{
	assert(session());

	Wt::Dbo::collection<std::string> copyrights = session()->query<std::string>
		("SELECT copyright_url FROM track t INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?")
		.groupBy("copyright_url")
		.bind(getId());

	std::vector<std::string> values(copyrights.begin(), copyrights.end());

	// various copyright URLs => no copyright URL
	if (values.empty() || values.size() > 1 || values.front().empty())
		return std::nullopt;

	return values.front();
}

std::vector<Artist::pointer>
Release::getArtists(TrackArtistLinkType linkType) const
{
	assert(session());

	auto res {session()->query<Wt::Dbo::ptr<Artist>>(
			"SELECT DISTINCT a FROM artist a"
			" INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
			" INNER JOIN track t ON t.id = t_a_l.track_id"
			" INNER JOIN release r ON r.id = t.release_id")
		.where("r.id = ?").bind(getId())
		.where("t_a_l.type = ?").bind(linkType)
		.resultList()};

	return std::vector<Artist::pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
Release::getSimilarReleases(std::optional<std::size_t> offset, std::optional<std::size_t> count) const
{
	assert(session());

	auto res {session()->query<Wt::Dbo::ptr<Release>>(
			"SELECT r FROM release r"
			" INNER JOIN track t ON t.release_id = r.id"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
				" WHERE "
					" t_c.cluster_id IN (SELECT c.id from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN release r ON r.id = t.release_id WHERE r.id = ?)"
					" AND r.id <> ?"
				)
		.bind(getId())
		.bind(getId())
		.groupBy("r.id")
		.orderBy("COUNT(*) DESC, RANDOM()")
		.limit(count ? static_cast<int>(*count) : -1)
		.offset(offset ? static_cast<int>(*offset) : -1)
		.resultList()};

	return std::vector<pointer>(res.begin(), res.end());
}

bool
Release::hasVariousArtists() const
{
	// TODO optimize
	return getArtists().size() > 1;
}

std::vector<Track::pointer>
Release::getTracks(const std::vector<ClusterId>& clusterIds) const
{
	assert(session());

	WhereClause where;

	std::ostringstream oss;
	oss << "SELECT t FROM track t INNER JOIN release r ON t.release_id = r.id";

	if (!clusterIds.empty())
	{
		oss << " INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;

		for (auto id : clusterIds)
			clusterClause.Or(WhereClause("c.id = ?")).bind(id.toString());

		where.And(clusterClause);
	}

	where.And(WhereClause("r.id = ?")).bind(getId().toString());

	oss << " " << where.get();

	if (!clusterIds.empty())
		oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size();

	oss << " ORDER BY t.disc_number,t.track_number";

	auto query {session()->query<Wt::Dbo::ptr<Track>>(oss.str())};
	for (const std::string& bindArg : where.getBindArgs())
		query.bind(bindArg);

	auto res {query.resultList()};
	return std::vector<Track::pointer> (res.begin(), res.end());
}

std::size_t
Release::getTracksCount() const
{
	return _tracks.size();
}

Track::pointer
Release::getFirstTrack() const
{
	assert(session());

	return session()->query<Wt::Dbo::ptr<Track>>("SELECT t from track t")
		.join("release r ON t.release_id = r.id")
		.where("r.id = ?").bind(getId())
		.orderBy("t.disc_number,t.track_number")
		.limit(1)
		.resultValue();
}

std::chrono::milliseconds
Release::getDuration() const
{
	assert(session());

	using milli = std::chrono::duration<int, std::milli>;

	Wt::Dbo::Query<milli> query {session()->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t INNER JOIN release r ON t.release_id = r.id")
			.where("r.id = ?").bind(getId())};

	return query.resultValue();
}

Wt::WDateTime
Release::getLastWritten() const
{
	assert(session());

	Wt::Dbo::Query<Wt::WDateTime> query {session()->query<Wt::WDateTime>("SELECT COALESCE(MAX(file_last_write), '1970-01-01T00:00:00') FROM track t INNER JOIN release r ON t.release_id = r.id")
			.where("r.id = ?").bind(getId())};

	return query.resultValue();
}

std::vector<std::vector<Cluster::pointer>>
Release::getClusterGroups(const std::vector<ClusterType::pointer>& clusterTypes, std::size_t size) const
{
	assert(session());

	WhereClause where;

	std::ostringstream oss;

	oss << "SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN cluster_type c_type ON c.cluster_type_id = c_type.id INNER JOIN release r ON t.release_id = r.id ";

	where.And(WhereClause("r.id = ?")).bind(getId().toString());
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

	std::map<ClusterTypeId, std::vector<Cluster::pointer>> clustersByType;
	for (const Wt::Dbo::ptr<Cluster>& cluster : queryRes)
	{
		if (clustersByType[cluster->getType()->getId()].size() < size)
			clustersByType[cluster->getType()->getId()].push_back(cluster);
	}

	std::vector<std::vector<Cluster::pointer>> res;
	for (const auto& [clusterTypeId, clusters] : clustersByType)
		res.push_back(clusters);

	return res;
}

} // namespace Database
