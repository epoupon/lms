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

#include "services/database/Track.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Release.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "services/database/TrackFeatures.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "utils/Logger.hpp"

#include "IdTypeTraits.hpp"
#include "SqlQuery.hpp"
#include "StringViewTraits.hpp"
#include "Utils.hpp"

namespace Database {

static
Wt::Dbo::Query<TrackId>
createQuery(Session& session, const Track::FindParameters& params)
{
	session.checkSharedLocked();

	auto query {session.getDboSession().query<TrackId>(params.distinct ? "SELECT DISTINCT t.id FROM track t" : "SELECT t.id FROM track t")};

	assert(params.keywords.empty() || params.name.empty());
	for (std::string_view keyword : params.keywords)
		query.where("t.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + Utils::escapeLikeKeyword(keyword) + "%");

	if (!params.name.empty())
		query.where("t.name = ?").bind(params.name);

	if (params.writtenAfter.isValid())
		query.where("t.file_last_write > ?").bind(params.writtenAfter);

	if (params.starringUser.isValid())
	{
		assert(params.scrobbler);
		query.join("starred_track s_t ON s_t.track_id = t.id")
			.where("s_t.user_id = ?").bind(params.starringUser)
			.where("s_t.scrobbler = ?").bind(*params.scrobbler)
			.where("s_t.scrobbling_state <> ?").bind(ScrobblingState::PendingRemove);
	}

	if (!params.clusters.empty())
	{
		std::ostringstream oss;
		oss << "t.id IN (SELECT DISTINCT t.id FROM track t"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
			" INNER JOIN cluster c ON c.id = t_c.cluster_id";

		WhereClause clusterClause;
		for (const ClusterId clusterId : params.clusters)
		{
			clusterClause.Or(WhereClause("c.id = ?"));
			query.bind(clusterId);
		}

		oss << " " << clusterClause.get();
		oss << " GROUP BY t.id HAVING COUNT(*) = " << params.clusters.size() << ")";

		query.where(oss.str());
	}

	if (params.artist.isValid() || !params.artistName.empty())
	{
		query.join("artist a ON a.id = t_a_l.artist_id")
				.join("track_artist_link t_a_l ON t_a_l.track_id = t.id");

		if (params.artist.isValid())
			query.where("a.id = ?").bind(params.artist);
		if (!params.artistName.empty())
			query.where("a.name = ?").bind(params.artistName);

		if (!params.trackArtistLinkTypes.empty())
		{
			std::ostringstream oss;

			bool first {true};
			for (TrackArtistLinkType linkType : params.trackArtistLinkTypes)
			{
				if (!first)
					oss << " OR ";
				oss << "t_a_l.type = ?";
				query.bind(linkType);

				first = false;
			}
			query.where(oss.str());
		}
	}

	assert(!(params.nonRelease && params.release.isValid()));
	if (params.nonRelease)
		query.where("t.release_id IS NULL");
	else if (params.release.isValid())
		query.where("t.release_id = ?").bind(params.release);
	else if (!params.releaseName.empty())
	{
		query.join("release r ON t.release_id = r.id");
		query.where("r.name = ?").bind(params.releaseName);
	}

	if (params.trackList.isValid())
	{
		query.join("tracklist t_l ON t_l_e.tracklist_id = t_l.id");
		query.join("tracklist_entry t_l_e ON t.id = t_l_e.track_id");
		query.where("t_l.id = ?").bind(params.trackList);
	}

	if (params.trackNumber)
		query.where("t.track_number = ?").bind(*params.trackNumber);

	switch (params.sortMethod)
	{
		case TrackSortMethod::None:
			break;
		case TrackSortMethod::LastWritten:
			query.orderBy("t.file_last_write DESC");
			break;
		case TrackSortMethod::Random:
			query.orderBy("RANDOM()");
			break;
		case TrackSortMethod::StarredDateDesc:
			assert(params.starringUser.isValid());
			query.orderBy("s_t.date_time DESC");
			break;
		case TrackSortMethod::Name:
			query.orderBy("t.name COLLATE NOCASE");
			break;
		case TrackSortMethod::DateDescAndRelease:
			query.orderBy("t.date DESC,t.release_id,t.disc_number,t.track_number");
			break;
		case TrackSortMethod::Release:
			query.orderBy("t.disc_number,t.track_number");
			break;
		case TrackSortMethod::TrackList:
			assert(params.trackList.isValid());
			query.orderBy("t_l.id");
	}

	return query;
}

Track::Track(const std::filesystem::path& p)
: _filePath {p.string()}
{
}

Track::pointer
Track::create(Session& session, const std::filesystem::path& p)
{
	return session.getDboSession().add(std::unique_ptr<Track> {new Track {p}});
}

std::size_t
Track::getCount(Session& session)
{
	session.checkSharedLocked();

	return session.getDboSession().query<int>("SELECT COUNT(*) FROM track");
}

Track::pointer
Track::findByPath(Session& session, const std::filesystem::path& p)
{
	session.checkSharedLocked();

	return session.getDboSession().find<Track>().where("file_path = ?").bind(p.string()).resultValue();
}

Track::pointer
Track::find(Session& session, TrackId id)
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
Track::findByMBID(Session& session, const UUID& mbid)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<Track>()
		.where("mbid = ?").bind(std::string {mbid.getAsString()})
		.resultList()};

	return std::vector<Track::pointer>(res.begin(), res.end());
}

std::vector<Track::pointer>
Track::findByRecordingMBID(Session& session, const UUID& mbid)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<Track>()
		.where("recording_mbid = ?").bind(std::string {mbid.getAsString()})
		.resultList()};

	return std::vector<Track::pointer>(res.begin(), res.end());
}

RangeResults<Track::PathResult>
Track::findPaths(Session& session, Range range)
{
	using QueryResultType = std::tuple<TrackId, std::string>;
	session.checkSharedLocked();

	// TODO Dbo traits on filesystem
	auto query {session.getDboSession().query<QueryResultType>("SELECT id, file_path FROM track")};

	RangeResults<QueryResultType> queryResults {Utils::execQuery(query, range)};

	RangeResults<PathResult> res;
	res.range = queryResults.range;
	res.moreResults = queryResults.moreResults;
	res.results.reserve(queryResults.results.size());

	std::transform(std::cbegin(queryResults.results), std::cend(queryResults.results), std::back_inserter(res.results),
			[](const QueryResultType& queryResult)
			{
				return PathResult {std::get<0>(queryResult), std::get<1>(queryResult)};
			});

	return res;
}

RangeResults<TrackId>
Track::findTrackMBIDDuplicates(Session& session, Range range)
{
	session.checkSharedLocked();

	auto query {session.getDboSession().query<TrackId>( "SELECT track.id FROM track WHERE mbid in (SELECT mbid FROM track WHERE mbid <> '' GROUP BY mbid HAVING COUNT (*) > 1)")
		.orderBy("track.release_id,track.disc_number,track.track_number,track.mbid")};

	return Utils::execQuery(query, range);
}

RangeResults<TrackId>
Track::findWithRecordingMBIDAndMissingFeatures(Session& session, Range range)
{
	session.checkSharedLocked();

	auto query {session.getDboSession().query<TrackId>("SELECT t.id FROM track t")
		.where("LENGTH(t.recording_mbid) > 0")
		.where("NOT EXISTS (SELECT * FROM track_features t_f WHERE t_f.track_id = t.id)")};

	return Utils::execQuery(query, range);
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

RangeResults<TrackId>
Track::find(Session& session, const FindParameters& parameters)
{
	session.checkSharedLocked();

	auto query {createQuery(session, parameters)};

	return Utils::execQuery(query, parameters.range);
}

RangeResults<TrackId>
Track::findSimilarTracks(Session& session, const std::vector<TrackId>& tracks, Range range)
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

	auto query {session.getDboSession().query<TrackId>(
			"SELECT t.id FROM track t"
			" INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
					" AND t_c.cluster_id IN (SELECT c.id FROM cluster c INNER JOIN track_cluster t_c ON t_c.cluster_id = c.id WHERE t_c.track_id IN (" + oss.str() + "))"
					" AND t.id NOT IN (" + oss.str() + ")")
		.groupBy("t.id")
		.orderBy("COUNT(*) DESC, RANDOM()")};

	for (TrackId trackId : tracks)
		query.bind(trackId);

	for (TrackId trackId : tracks)
		query.bind(trackId);

	return Utils::execQuery(query, range);
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
		for ([[maybe_unused]] TrackArtistLinkType type : linkTypes)
		{
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
		"SELECT DISTINCT a.id from artist a"
		" INNER JOIN track_artist_link t_a_l ON a.id = t_a_l.artist_id"
		" INNER JOIN track t ON t.id = t_a_l.track_id";

	if (!linkTypes.empty())
	{
		oss << " AND t_a_l.type IN (";

		bool first {true};
		for ([[maybe_unused]] TrackArtistLinkType type : linkTypes)
		{
			if (!first)
				oss << ", ";
			oss << "?";
			first = false;
		}
		oss << ")";
	}

	auto query {session()->query<ArtistId>(oss.str())};
	for (TrackArtistLinkType type : linkTypes)
		query.bind(type);

	query.where("t.id = ?").bind(getId());

	auto res {query.resultList()};
	return std::vector<ArtistId>(std::cbegin(res), std::cend(res));
}

std::vector<TrackArtistLink::pointer>
Track::getArtistLinks() const
{
	return std::vector<TrackArtistLink::pointer>(_trackArtistLinks.begin(), _trackArtistLinks.end());
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

namespace Debug
{
	std::ostream&
	operator<<(std::ostream& os, const TrackInfo& trackInfo)
	{
		auto transaction {trackInfo.session.createSharedTransaction()};

		const Track::pointer track {Track::find(trackInfo.session, trackInfo.trackId)};
		if (track)
		{
			os << track->getName();

			if (const Release::pointer release {track->getRelease()})
				os << " [" << release->getName() << "]";
			for (auto artist : track->getArtists({TrackArtistLinkType::Artist}))
				os << " - " << artist->getName();
			for (auto cluster : track->getClusters())
				os << " {" + cluster->getType()->getName() << "-" << cluster->getName() << "}";
		}
		else
		{
			os << "*unknown*";
		}

		return os;
	}
}

} // namespace Database

