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
#include "services/database/Artist.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "services/database/Cluster.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/Logger.hpp"
#include "SqlQuery.hpp"
#include "Utils.hpp"
#include "EnumSetTraits.hpp"
#include "IdTypeTraits.hpp"

namespace Database
{

Artist::Artist(const std::string& name, const std::optional<UUID>& MBID)
: _name {std::string(name, 0 , _maxNameLength)},
_sortName {_name},
_MBID {MBID ? MBID->getAsString() : ""}
{
}

Artist::pointer
Artist::create(Session& session, const std::string& name, const std::optional<UUID>& MBID)
{
	return session.getDboSession().add(std::unique_ptr<Artist> {new Artist {name, MBID}});
}

std::size_t
Artist::getCount(Session& session)
{
	session.checkSharedLocked();

	return session.getDboSession().query<int>("SELECT COUNT(*) FROM artist");
}

std::vector<Artist::pointer>
Artist::find(Session& session, const std::string& name)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<Wt::Dbo::ptr<Artist>> res = session.getDboSession().find<Artist>()
		.where("name = ?").bind(std::string {name, 0, _maxNameLength})
		.orderBy("LENGTH(mbid) DESC"); // put mbid entries first

	return std::vector<Artist::pointer>(res.begin(), res.end());
}

Artist::pointer
Artist::find(Session& session, const UUID& mbid)
{
	session.checkSharedLocked();
	return session.getDboSession().find<Artist>().where("mbid = ?").bind(std::string {mbid.getAsString()}).resultValue();
}

Artist::pointer
Artist::find(Session& session, ArtistId id)
{
	session.checkSharedLocked();
	return session.getDboSession().find<Artist>().where("id = ?").bind(id).resultValue();
}

bool
Artist::exists(Session& session, ArtistId id)
{
	session.checkSharedLocked();
	return session.getDboSession().query<int>("SELECT 1 FROM artist").where("id = ?").bind(id).resultValue() == 1;
}

static
Wt::Dbo::Query<ArtistId>
createQuery(Session& session, const Artist::FindParameters& params)
{
	session.checkSharedLocked();

	auto query {session.getDboSession().query<ArtistId>("SELECT DISTINCT a.id FROM artist a")};
	if (params.sortMethod == ArtistSortMethod::LastWritten
			|| params.writtenAfter.isValid()
			|| params.linkType
			|| params.track.isValid()
			|| params.release.isValid())
	{
		query.join("track t ON t.id = t_a_l.track_id");
		query.join("track_artist_link t_a_l ON t_a_l.artist_id = a.id");
	}

	if (params.linkType)
		query.where("t_a_l.type = ?").bind(*params.linkType);

	if (params.writtenAfter.isValid())
		query.where("t.file_last_write > ?").bind(params.writtenAfter);

	if (!params.keywords.empty())
	{
		std::vector<std::string> clauses;
		std::vector<std::string> sortClauses;

		for (std::string_view keyword : params.keywords)
		{
			clauses.push_back("a.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'");
			query.bind("%" + Utils::escapeLikeKeyword(keyword) + "%");
		}

		for (std::string_view keyword : params.keywords)
		{
			sortClauses.push_back("a.sort_name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'");
			query.bind("%" + Utils::escapeLikeKeyword(keyword) + "%");
		}

		query.where("(" + StringUtils::joinStrings(clauses, " AND ") + ") OR (" + StringUtils::joinStrings(sortClauses, " AND ") + ")");
	}

	if (params.starringUser.isValid())
	{
		assert(params.scrobbler);
		query.join("starred_artist s_a ON s_a.artist_id = a.id")
			.where("s_a.user_id = ?").bind(params.starringUser)
			.where("s_a.scrobbler = ?").bind(*params.scrobbler)
			.where("s_a.scrobbling_state <> ?").bind(ScrobblingState::PendingRemove);
	}

	if (!params.clusters.empty())
	{
		std::ostringstream oss;
		oss << "a.id IN (SELECT DISTINCT a.id FROM artist a"
			" INNER JOIN track t ON t.id = t_a_l.track_id"
			" INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
			" INNER JOIN cluster c ON c.id = t_c.cluster_id"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

		WhereClause clusterClause;
		for (const ClusterId clusterId : params.clusters)
		{
			clusterClause.Or(WhereClause("c.id = ?"));
			query.bind(clusterId);
		}

		oss << " " << clusterClause.get();
		oss << " GROUP BY t.id,a.id HAVING COUNT(DISTINCT c.id) = " << params.clusters.size() << ")";

		query.where(oss.str());
	}

	if (params.track.isValid())
		query.where("t.id = ?").bind(params.track);

	if (params.release.isValid())
		query.where("t.release_id = ?").bind(params.release);

	switch (params.sortMethod)
	{
		case ArtistSortMethod::None:
			break;
		case ArtistSortMethod::ByName:
			query.orderBy("a.name COLLATE NOCASE");
			break;
		case ArtistSortMethod::BySortName:
			query.orderBy("a.sort_name COLLATE NOCASE");
			break;
		case ArtistSortMethod::Random:
			query.orderBy("RANDOM()");
			break;
		case ArtistSortMethod::LastWritten:
			query.orderBy("t.file_last_write DESC");
			break;
		case ArtistSortMethod::StarredDateDesc:
			assert(params.starringUser.isValid());
			query.orderBy("s_a.date_time DESC");
			break;
	}

	return query;
}

RangeResults<ArtistId>
Artist::findAllOrphans(Session& session, Range range)
{
	session.checkSharedLocked();
	auto query {session.getDboSession().query<ArtistId>("SELECT DISTINCT a.id FROM artist a WHERE NOT EXISTS(SELECT 1 FROM track t INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id WHERE t.id = t_a_l.track_id)")};

	return Utils::execQuery(query, range);
}

RangeResults<ArtistId>
Artist::find(Session& session, const FindParameters& params)
{
	session.checkSharedLocked();

	auto query {createQuery(session, params)};
	return Utils::execQuery(query, params.range);
}

RangeResults<ArtistId>
Artist::findSimilarArtists(EnumSet<TrackArtistLinkType> artistLinkTypes, Range range) const
{
	assert(session());

	std::ostringstream oss;
	oss <<
			"SELECT a.id FROM artist a"
			" INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
			" INNER JOIN track t ON t.id = t_a_l.track_id"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
				" WHERE "
					" t_c.cluster_id IN (SELECT c.id from cluster c"
											" INNER JOIN track t ON c.id = t_c.cluster_id"
											" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
											" INNER JOIN artist a ON a.id = t_a_l.artist_id"
											" INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id"
											" WHERE a.id = ?)"
					" AND a.id <> ?";

	if (!artistLinkTypes.empty())
	{
		oss << " AND t_a_l.type IN (";

		bool first {true};
		for (TrackArtistLinkType type : artistLinkTypes)
		{
			(void) type;
			if (!first)
				oss << ", ";
			oss << "?";
			first = false;
		}
		oss << ")";
	}

	auto query {session()->query<ArtistId>(oss.str())
		.bind(getId())
		.bind(getId())
		.groupBy("a.id")
		.orderBy("COUNT(*) DESC, RANDOM()")};

	for (TrackArtistLinkType type : artistLinkTypes)
		query.bind(type);

	return Utils::execQuery(query, range);
}

std::vector<std::vector<Cluster::pointer>>
Artist::getClusterGroups(std::vector<ClusterType::pointer> clusterTypes, std::size_t size) const
{
	assert(session());

	WhereClause where;

	std::ostringstream oss;
	oss << "SELECT c FROM cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN cluster_type c_type ON c.cluster_type_id = c_type.id INNER JOIN artist a ON t_a_l.artist_id = a.id INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id";

	where.And(WhereClause("a.id = ?")).bind(getId().toString());
	{
		WhereClause clusterClause;
		for (auto clusterType : clusterTypes)
			clusterClause.Or(WhereClause("c_type.id = ?")).bind(clusterType->getId().toString());

		where.And(clusterClause);
	}
	oss << " " << where.get();
	oss << "GROUP BY c.id ORDER BY COUNT(DISTINCT c.id) DESC";

	Wt::Dbo::Query<Wt::Dbo::ptr<Cluster>> query = session()->query<Wt::Dbo::ptr<Cluster>>( oss.str() );

	for (const std::string& bindArg : where.getBindArgs())
		query.bind(bindArg);

	Wt::Dbo::collection<Wt::Dbo::ptr<Cluster>> queryRes = query;

	std::map<ClusterTypeId, std::vector<Cluster::pointer>> clustersByType;
	for (Cluster::pointer cluster : queryRes)
	{
		if (clustersByType[cluster->getType()->getId()].size() < size)
			clustersByType[cluster->getType()->getId()].push_back(cluster);
	}

	std::vector<std::vector<Cluster::pointer>> res;
	for (const auto& [clusterTypeId, clusters] : clustersByType)
		res.push_back(clusters);

	return res;
}

void
Artist::setSortName(const std::string& sortName)
{
	_sortName = std::string(sortName, 0 , _maxNameLength);
}

} // namespace Database
