/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "services/database/Listen.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "IdTypeTraits.hpp"
#include "SqlQuery.hpp"
#include "Utils.hpp"

namespace
{
    using namespace Database;

    Wt::Dbo::Query<ArtistId> createArtistsQuery(Wt::Dbo::Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType)
    {
        auto query{ session.query<ArtistId>("SELECT a.id from artist a")
                        .join("track t ON t.id = t_a_l.track_id")
                        .join("track_artist_link t_a_l ON t_a_l.artist_id = a.id")
                        .join("listen l ON l.track_id = t.id")
                        .where("l.user_id = ?").bind(userId)
                        .where("l.backend = ?").bind(backend) };

        if (linkType)
            query.where("t_a_l.type = ?").bind(*linkType);

        if (!clusterIds.empty())
        {
            std::ostringstream oss;
            oss << "a.id IN (SELECT DISTINCT a.id FROM artist a"
                " INNER JOIN track t ON t.id = t_a_l.track_id"
                " INNER JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id"
                " INNER JOIN cluster c ON c.id = t_c.cluster_id"
                " INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

            WhereClause clusterClause;
            for (auto id : clusterIds)
            {
                clusterClause.Or(WhereClause("c.id = ?"));
                query.bind(id);
            }

            oss << " " << clusterClause.get();
            oss << " GROUP BY t.id,a.id HAVING COUNT(DISTINCT c.id) = " << clusterIds.size() << ")";

            query.where(oss.str());
        }

        return query;
    }

    Wt::Dbo::Query<ReleaseId> createReleasesQuery(Wt::Dbo::Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds)
    {
        auto query{ session.query<ReleaseId>("SELECT r.id from release r")
                        .join("track t ON t.release_id = r.id")
                        .join("listen l ON l.track_id = t.id")
                        .where("l.user_id = ?").bind(userId)
                        .where("l.backend = ?").bind(backend) };

        if (!clusterIds.empty())
        {
            std::ostringstream oss;
            oss << "r.id IN (SELECT DISTINCT r.id FROM release r"
                " INNER JOIN track t ON t.release_id = r.id"
                " INNER JOIN cluster c ON c.id = t_c.cluster_id"
                " INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

            WhereClause clusterClause;
            for (ClusterId id : clusterIds)
            {
                clusterClause.Or(WhereClause("c.id = ?"));
                query.bind(id);
            }

            oss << " " << clusterClause.get();
            oss << " GROUP BY t.id HAVING COUNT(DISTINCT c.id) = " << clusterIds.size() << ")";

            query.where(oss.str());
        }

        return query;
    }

    Wt::Dbo::Query<TrackId> createTracksQuery(Wt::Dbo::Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds)
    {
        auto query{ session.query<TrackId>("SELECT t.id from track t")
                    .join("listen l ON l.track_id = t.id")
                    .where("l.user_id = ?").bind(userId)
                    .where("l.backend = ?").bind(backend) };

        if (!clusterIds.empty())
        {
            std::ostringstream oss;
            oss << "t.id IN (SELECT DISTINCT t.id FROM track t"
                " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
                " INNER JOIN cluster c ON c.id = t_c.cluster_id";

            WhereClause clusterClause;
            for (auto id : clusterIds)
            {
                clusterClause.Or(WhereClause("c.id = ?")).bind(id.toString());
                query.bind(id);
            }

            oss << " " << clusterClause.get();
            oss << " GROUP BY t.id HAVING COUNT(*) = " << clusterIds.size() << ")";

            query.where(oss.str());
        }

        return query;
    }
}

namespace Database
{
    Listen::Listen(ObjectPtr<User> user, ObjectPtr<Track> track, ScrobblingBackend backend, const Wt::WDateTime& dateTime)
        : _dateTime{ Wt::WDateTime::fromTime_t(dateTime.toTime_t()) }
        , _backend{ backend }
        , _user{ getDboPtr(user) }
        , _track{ getDboPtr(track) }
    {}

    Listen::pointer Listen::create(Session& session, ObjectPtr<User> user, ObjectPtr<Track> track, ScrobblingBackend backend, const Wt::WDateTime& dateTime)
    {
        session.checkWriteTransaction();
        return session.getDboSession().add(std::unique_ptr<Listen> {new Listen{ user, track, backend, dateTime }});
    }

    std::size_t Listen::getCount(Session& session)
    {
        session.checkReadTransaction();
        return session.getDboSession().query<int>("SELECT COUNT(*) FROM listen");
    }

    Listen::pointer Listen::find(Session& session, ListenId id)
    {
        session.checkReadTransaction();
        return session.getDboSession().find<Listen>().where("id = ?").bind(id).resultValue();
    }

    RangeResults<ListenId> Listen::find(Session& session, const FindParameters& parameters)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession().query<ListenId>("SELECT id FROM listen")
                                .orderBy("date_time") };

        if (parameters.user.isValid())
            query.where("user_id = ?").bind(parameters.user);

        if (parameters.backend)
            query.where("backend = ?").bind(*parameters.backend);

        if (parameters.syncState)
            query.where("sync_state = ?").bind(*parameters.syncState);

        return Utils::execQuery<ListenId>(query, parameters.range);
    }

    Listen::pointer Listen::find(Session& session, UserId userId, TrackId trackId, ScrobblingBackend backend, const Wt::WDateTime& dateTime)
    {
        session.checkReadTransaction();

        return session.getDboSession().find<Listen>()
            .where("user_id = ?").bind(userId)
            .where("track_id = ?").bind(trackId)
            .where("backend = ?").bind(backend)
            .where("date_time = ?").bind(Wt::WDateTime::fromTime_t(dateTime.toTime_t()))
            .resultValue();
    }

    RangeResults<ArtistId> Listen::getTopArtists(Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ createArtistsQuery(session.getDboSession(), userId, backend, clusterIds, linkType) };

        auto collection{ query
            .orderBy("COUNT(a.id) DESC")
            .groupBy("a.id") };

        return Utils::execQuery<ArtistId>(query, range);
    }

    RangeResults<ReleaseId> Listen::getTopReleases(Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ createReleasesQuery(session.getDboSession(), userId, backend, clusterIds)
                        .orderBy("COUNT(r.id) DESC")
                        .groupBy("r.id") };

        return Utils::execQuery<ReleaseId>(query, range);
    }

    RangeResults<TrackId> Listen::getTopTracks(Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ createTracksQuery(session.getDboSession(), userId, backend, clusterIds)
                        .orderBy("COUNT(t.id) DESC")
                        .groupBy("t.id") };

        return Utils::execQuery<TrackId>(query, range);
    }

    RangeResults<ArtistId> Listen::getRecentArtists(Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ createArtistsQuery(session.getDboSession(), userId, backend, clusterIds, linkType)
                        .groupBy("a.id").having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        return Utils::execQuery<ArtistId>(query, range);
    }

    RangeResults<ReleaseId> Listen::getRecentReleases(Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ createReleasesQuery(session.getDboSession(), userId, backend, clusterIds)
                        .groupBy("r.id").having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        return Utils::execQuery<ReleaseId>(query, range);
    }

    RangeResults<TrackId> Listen::getRecentTracks(Session& session, UserId userId, ScrobblingBackend backend, const std::vector<ClusterId>& clusterIds, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ createTracksQuery(session.getDboSession(), userId, backend, clusterIds)
                        .groupBy("t.id").having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        return Utils::execQuery<TrackId>(query, range);
    }

    std::size_t Listen::getCount(Session& session, UserId userId, TrackId trackId)
    {
        session.checkReadTransaction();

        return session.getDboSession().query<int>("SELECT COUNT(*) from listen l")
            .join("user u ON u.id = l.user_id")
            .where("l.track_id = ?").bind(trackId)
            .where("l.user_id = ?").bind(userId)
            .where("l.backend = u.scrobbling_backend")
            .resultValue();
    }

    std::size_t Listen::getCount(Session& session, UserId userId, ReleaseId releaseId)
    {
        session.checkReadTransaction();

        return session.getDboSession().query<int>(
            "SELECT IFNULL(MIN(count_result), 0)"
            " FROM ("
            " SELECT COUNT(l.track_id) AS count_result"
            " FROM track t"
            " LEFT JOIN listen l ON t.id = l.track_id AND l.backend = (SELECT scrobbling_backend FROM user WHERE id = ?) AND l.user_id = ?"
            " WHERE t.release_id = ?"
            " GROUP BY t.id)")
            .bind(userId)
            .bind(userId)
            .bind(releaseId)
            .resultValue();
    }

    Listen::pointer Listen::getMostRecentListen(Session& session, UserId userId, ScrobblingBackend backend, ReleaseId releaseId)
    {
        session.checkReadTransaction();

        // TODO not pending remove?
        return session.getDboSession().query<Wt::Dbo::ptr<Listen>>("SELECT l from listen l")
            .join("track t ON l.track_id = t.id")
            .where("t.release_id = ?").bind(releaseId)
            .where("l.user_id = ?").bind(userId)
            .where("l.backend = ?").bind(backend)
            .orderBy("l.date_time DESC")
            .limit(1)
            .resultValue();
    }

    Listen::pointer Listen::getMostRecentListen(Session& session, UserId userId, ScrobblingBackend backend, TrackId trackId)
    {
        session.checkReadTransaction();
        // TODO not pending remove?
        return session.getDboSession().query<Wt::Dbo::ptr<Listen>>("SELECT l from listen l")
            .where("l.track_id = ?").bind(trackId)
            .where("l.user_id = ?").bind(userId)
            .where("l.backend = ?").bind(backend)
            .orderBy("l.date_time DESC")
            .limit(1)
            .resultValue();
    }
} // namespace Database

