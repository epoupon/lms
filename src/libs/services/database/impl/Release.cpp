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
#include "EnumSetTraits.hpp"
#include "IdTypeTraits.hpp"
#include "Utils.hpp"

namespace Database
{

    Wt::Dbo::Query<ReleaseId> createQuery(Session& session, const Release::FindParameters& params)
    {
        auto query{ session.getDboSession().query<ReleaseId>("SELECT DISTINCT r.id from release r") };

        if (params.sortMethod == ReleaseSortMethod::LastWritten
            || params.sortMethod == ReleaseSortMethod::Date
            || params.sortMethod == ReleaseSortMethod::OriginalDate
            || params.sortMethod == ReleaseSortMethod::OriginalDateDesc
            || params.writtenAfter.isValid()
            || params.dateRange
            || params.artist.isValid())
        {
            query.join("track t ON t.release_id = r.id");
        }

        if (params.writtenAfter.isValid())
            query.where("t.file_last_write > ?").bind(params.writtenAfter);

        if (params.dateRange)
        {
            query.where("t.date >= ?").bind(params.dateRange->begin);
            query.where("t.date <= ?").bind(params.dateRange->end);
        }

        for (std::string_view keyword : params.keywords)
            query.where("r.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + Utils::escapeLikeKeyword(keyword) + "%");

        if (params.starringUser.isValid())
        {
            assert(params.scrobbler);
            query.join("starred_release s_r ON s_r.release_id = r.id")
                .where("s_r.user_id = ?").bind(params.starringUser)
                .where("s_r.scrobbler = ?").bind(*params.scrobbler)
                .where("s_r.scrobbling_state <> ?").bind(ScrobblingState::PendingRemove);
        }

        if (params.artist.isValid())
        {
            query.join("artist a ON a.id = t_a_l.artist_id")
                .join("track_artist_link t_a_l ON t_a_l.track_id = t.id")
                .where("a.id = ?").bind(params.artist);

            if (!params.trackArtistLinkTypes.empty())
            {
                std::ostringstream oss;

                bool first{ true };
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

            if (!params.excludedTrackArtistLinkTypes.empty())
            {
                std::ostringstream oss;
                oss << "r.id NOT IN (SELECT DISTINCT r.id FROM release r"
                    " INNER JOIN artist a ON a.id = t_a_l.artist_id"
                    " INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id"
                    " INNER JOIN track t ON t.release_id = r.id"
                    " WHERE (a.id = ? AND (";

                query.bind(params.artist);

                bool first{ true };
                for (const TrackArtistLinkType linkType : params.excludedTrackArtistLinkTypes)
                {
                    if (!first)
                        oss << " OR ";
                    oss << "t_a_l.type = ?";
                    query.bind(linkType);

                    first = false;
                }
                oss << ")))";
                query.where(oss.str());
            }
        }

        if (!params.clusters.empty())
        {
            std::ostringstream oss;
            oss << "r.id IN (SELECT DISTINCT r.id FROM release r"
                " INNER JOIN track t ON t.release_id = r.id"
                " INNER JOIN cluster c ON c.id = t_c.cluster_id"
                " INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

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

        if (params.primaryType)
            query.where("primary_type = ?").bind(*params.primaryType);
        if (!params.secondaryTypes.empty())
            query.where("secondary_type = ?").bind(params.secondaryTypes);

        switch (params.sortMethod)
        {
        case ReleaseSortMethod::None:
            break;
        case ReleaseSortMethod::Name:
            query.orderBy("r.name COLLATE NOCASE");
            break;
        case ReleaseSortMethod::Random:
            query.orderBy("RANDOM()");
            break;
        case ReleaseSortMethod::LastWritten:
            query.orderBy("t.file_last_write DESC");
            break;
        case ReleaseSortMethod::Date:
            query.orderBy("t.date, r.name COLLATE NOCASE");
            break;
        case ReleaseSortMethod::OriginalDate:
            query.orderBy("CASE WHEN t.original_date IS NULL THEN t.date ELSE t.original_date END, t.date, r.name COLLATE NOCASE");
            break;
        case ReleaseSortMethod::OriginalDateDesc:
            query.orderBy("CASE WHEN t.original_date IS NULL THEN t.date ELSE t.original_date END DESC, t.date, r.name COLLATE NOCASE");
            break;
        case ReleaseSortMethod::StarredDateDesc:
            assert(params.starringUser.isValid());
            query.orderBy("s_r.date_time DESC");
            break;
        }

        return query;
    }

    Release::Release(const std::string& name, const std::optional<UUID>& MBID)
        : _name{ std::string(name, 0 , _maxNameLength) },
        _MBID{ MBID ? MBID->getAsString() : "" }
    {
    }

    Release::pointer Release::create(Session& session, const std::string& name, const std::optional<UUID>& MBID)
    {
        return session.getDboSession().add(std::unique_ptr<Release> {new Release{ name, MBID }});
    }

    std::vector<Release::pointer> Release::find(Session& session, const std::string& name)
    {
        session.checkUniqueLocked();

        auto res{ session.getDboSession()
                            .find<Release>()
                            .where("name = ?").bind(std::string(name, 0, _maxNameLength))
                            .resultList() };

        return std::vector<Release::pointer>(res.begin(), res.end());
    }

    Release::pointer Release::find(Session& session, const UUID& mbid)
    {
        session.checkSharedLocked();

        return session.getDboSession()
            .find<Release>()
            .where("mbid = ?").bind(std::string{ mbid.getAsString() })
            .resultValue();;
    }

    Release::pointer Release::find(Session& session, ReleaseId id)
    {
        session.checkSharedLocked();

        return session.getDboSession()
            .find<Release>()
            .where("id = ?").bind(id)
            .resultValue();
    }

    bool Release::exists(Session& session, ReleaseId id)
    {
        session.checkSharedLocked();
        return session.getDboSession().query<int>("SELECT 1 FROM release").where("id = ?").bind(id).resultValue() == 1;
    }

    std::size_t Release::getCount(Session& session)
    {
        session.checkSharedLocked();

        return session.getDboSession().query<int>("SELECT COUNT(*) FROM release");
    }

    RangeResults<ReleaseId> Release::findOrderedByArtist(Session& session, Range range)
    {
        session.checkSharedLocked();

        // TODO merge with find
        auto query{ session.getDboSession().query<ReleaseId>(
                "SELECT DISTINCT r.id FROM release r"
                " INNER JOIN track t ON r.id = t.release_id"
                " INNER JOIN track_artist_link t_a_l ON t_a_l.track_id = t.id"
                " INNER JOIN artist a ON t_a_l.artist_id = a.id")
             .orderBy("a.name COLLATE NOCASE, r.name COLLATE NOCASE") };

        return Utils::execQuery(query, range);
    }

    RangeResults<ReleaseId> Release::findOrphans(Session& session, Range range)
    {
        session.checkSharedLocked();

        auto query{ session.getDboSession().query<ReleaseId>("select r.id from release r LEFT OUTER JOIN Track t ON r.id = t.release_id WHERE t.id IS NULL") };
        return Utils::execQuery(query, range);
    }

    RangeResults<ReleaseId> Release::find(Session& session, const FindParameters& params)
    {
        session.checkSharedLocked();

        auto query{ createQuery(session, params) };

        return Utils::execQuery(query, params.range);
    }

    std::size_t Release::getDiscCount() const
    {
        assert(session());
        int res{ session()->query<int>("SELECT COUNT(DISTINCT disc_number) FROM track t")
            .join("release r ON r.id = t.release_id")
            .where("r.id = ?")
            .bind(getId()) };

        return res;
    }

    std::vector<DiscInfo> Release::getDiscs() const
    {
        assert(session());
        using ResultType = std::tuple<int, std::string>;
        auto results{ session()->query<ResultType>("SELECT DISTINCT disc_number, disc_subtitle FROM track t")
            .join("release r ON r.id = t.release_id")
            .where("r.id = ?")
            .orderBy("disc_number")
            .bind(getId())
            .resultList() };

        std::vector<DiscInfo> discs;
        for (const auto& res : results)
            discs.emplace_back(DiscInfo{ static_cast<std::size_t>(std::get<int>(res)), std::get<std::string>(res) });

        return discs;
    }

    Wt::WDate Release::getReleaseDate() const
    {
        return getReleaseDate(false);
    }

    Wt::WDate Release::getOriginalReleaseDate() const
    {
        return getReleaseDate(true);
    }

    Wt::WDate Release::getReleaseDate(bool original) const
    {
        assert(session());

        const char* field{ original ? "original_date" : "date" };

        auto dates{ session()->query<Wt::WDate>(
                std::string {"SELECT "} + "t." + field + " FROM track t INNER JOIN release r ON r.id = t.release_id")
            .where("r.id = ?")
            .groupBy(field)
            .bind(getId())
            .resultList() };

        // various dates => invalid date
        if (dates.empty() || dates.size() > 1)
            return {};

        return dates.front();
    }

    std::optional<std::string> Release::getCopyright() const
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

    std::optional<std::string> Release::getCopyrightURL() const
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

    std::vector<Artist::pointer> Release::getArtists(TrackArtistLinkType linkType) const
    {
        assert(session());

        auto res{ session()->query<Wt::Dbo::ptr<Artist>>(
                "SELECT DISTINCT a FROM artist a"
                " INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
                " INNER JOIN track t ON t.id = t_a_l.track_id"
                " INNER JOIN release r ON r.id = t.release_id")
            .where("r.id = ?").bind(getId())
            .where("t_a_l.type = ?").bind(linkType)
            .resultList() };

        return std::vector<Artist::pointer>(res.begin(), res.end());
    }

    std::vector<Release::pointer> Release::getSimilarReleases(std::optional<std::size_t> offset, std::optional<std::size_t> count) const
    {
        assert(session());

        auto res{ session()->query<Wt::Dbo::ptr<Release>>(
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
            .resultList() };

        return std::vector<pointer>(res.begin(), res.end());
    }

    bool Release::hasVariousArtists() const
    {
        // TODO optimize
        return getArtists().size() > 1;
    }

    std::size_t Release::getTracksCount() const
    {
        return _tracks.size();
    }

    std::chrono::milliseconds Release::getDuration() const
    {
        assert(session());

        using milli = std::chrono::duration<int, std::milli>;

        Wt::Dbo::Query<milli> query{ session()->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t INNER JOIN release r ON t.release_id = r.id")
                .where("r.id = ?").bind(getId()) };

        return query.resultValue();
    }

    Wt::WDateTime Release::getLastWritten() const
    {
        assert(session());

        Wt::Dbo::Query<Wt::WDateTime> query{ session()->query<Wt::WDateTime>("SELECT COALESCE(MAX(file_last_write), '1970-01-01T00:00:00') FROM track t INNER JOIN release r ON t.release_id = r.id")
                .where("r.id = ?").bind(getId()) };

        return query.resultValue();
    }

    std::vector<std::vector<Cluster::pointer>> Release::getClusterGroups(const std::vector<ClusterType::pointer>& clusterTypes, std::size_t size) const
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

        auto query{ session()->query<Wt::Dbo::ptr<Cluster>>(oss.str()) };
        for (const std::string& bindArg : where.getBindArgs())
            query.bind(bindArg);

        auto queryRes{ query.resultList() };

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
