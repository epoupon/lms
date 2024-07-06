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

#include "database/Listen.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"

#include "IdTypeTraits.hpp"
#include "SqlQuery.hpp"
#include "Utils.hpp"

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<ArtistId> createArtistsQuery(Session& session, const Listen::ArtistStatsFindParameters& params)
        {
            auto query{ session.getDboSession()->query<ArtistId>("SELECT a.id from artist a").join("track_artist_link t_a_l ON t_a_l.artist_id = a.id").join("listen l ON l.track_id = t_a_l.track_id") };

            if (params.user.isValid())
                query.where("l.user_id = ?").bind(params.user);

            if (params.backend)
                query.where("l.backend = ?").bind(*params.backend);

            assert(!params.artist.isValid()); // poor check

            if (params.library.isValid())
            {
                query.join("track t ON t.id = t_a_l.track_id");
                query.where("t.media_library_id = ?").bind(params.library);
            }

            if (params.linkType)
                query.where("t_a_l.type = ?").bind(*params.linkType);

            if (!params.clusters.empty())
            {
                std::ostringstream oss;
                oss << "a.id IN (SELECT DISTINCT t_a_l.artist_id FROM track_artist_link t_a_l"
                       " INNER JOIN track_cluster t_c ON t_c.track_id = t_a_l.track_id";

                WhereClause clusterClause;
                for (auto id : params.clusters)
                {
                    clusterClause.Or(WhereClause("t_c.cluster_id = ?"));
                    query.bind(id);
                }

                oss << " " << clusterClause.get();
                oss << " GROUP BY t_a_l.track_id,t_a_l.artist_id HAVING COUNT(DISTINCT t_c.cluster_id) = " << params.clusters.size() << ")";

                query.where(oss.str());
            }

            if (!params.keywords.empty())
            {
                std::vector<std::string> clauses;
                std::vector<std::string> sortClauses;

                for (const std::string_view keyword : params.keywords)
                {
                    clauses.push_back("a.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'");
                    query.bind("%" + utils::escapeLikeKeyword(keyword) + "%");
                }

                for (const std::string_view keyword : params.keywords)
                {
                    sortClauses.push_back("a.sort_name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'");
                    query.bind("%" + utils::escapeLikeKeyword(keyword) + "%");
                }

                query.where("(" + core::stringUtils::joinStrings(clauses, " AND ") + ") OR (" + core::stringUtils::joinStrings(sortClauses, " AND ") + ")");
            }

            return query;
        }

        Wt::Dbo::Query<ReleaseId> createReleasesQuery(Session& session, const Listen::StatsFindParameters& params)
        {
            auto query{ session.getDboSession()->query<ReleaseId>("SELECT r.id from release r").join("track t ON t.release_id = r.id").join("listen l ON l.track_id = t.id") };

            if (params.user.isValid())
                query.where("l.user_id = ?").bind(params.user);

            if (params.backend)
                query.where("l.backend = ?").bind(*params.backend);

            if (params.artist.isValid())
            {
                query.join("track_artist_link t_a_l ON t_a_l.track_id = t.id")
                    .where("t_a_l.artist_id = ?")
                    .bind(params.artist);
            }

            if (params.library.isValid())
                query.where("t.media_library_id = ?").bind(params.library);

            if (!params.clusters.empty())
            {
                std::ostringstream oss;
                oss << "r.id IN (SELECT DISTINCT r.id FROM release r"
                       " INNER JOIN track t ON t.release_id = r.id"
                       " INNER JOIN cluster c ON c.id = t_c.cluster_id"
                       " INNER JOIN track_cluster t_c ON t_c.track_id = t.id";

                WhereClause clusterClause;
                for (ClusterId id : params.clusters)
                {
                    clusterClause.Or(WhereClause("c.id = ?"));
                    query.bind(id);
                }

                oss << " " << clusterClause.get();
                oss << " GROUP BY t.id HAVING COUNT(DISTINCT c.id) = " << params.clusters.size() << ")";

                query.where(oss.str());
            }

            for (std::string_view keyword : params.keywords)
                query.where("r.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + utils::escapeLikeKeyword(keyword) + "%");

            return query;
        }

        Wt::Dbo::Query<TrackId> createTracksQuery(Session& session, const Listen::StatsFindParameters& params)
        {
            auto query{ session.getDboSession()->query<TrackId>("SELECT t.id from track t").join("listen l ON l.track_id = t.id") };

            if (params.user.isValid())
                query.where("l.user_id = ?").bind(params.user);

            if (params.backend)
                query.where("l.backend = ?").bind(*params.backend);

            if (params.artist.isValid())
            {
                query.join("track_artist_link t_a_l ON t_a_l.track_id = t.id")
                    .where("t_a_l.artist_id = ?")
                    .bind(params.artist);
            }

            if (params.library.isValid())
                query.where("t.media_library_id = ?").bind(params.library);

            if (!params.clusters.empty())
            {
                std::ostringstream oss;
                oss << "t.id IN (SELECT DISTINCT t.id FROM track t"
                       " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
                       " INNER JOIN cluster c ON c.id = t_c.cluster_id";

                WhereClause clusterClause;
                for (auto id : params.clusters)
                {
                    clusterClause.Or(WhereClause("c.id = ?")).bind(id.toString());
                    query.bind(id);
                }

                oss << " " << clusterClause.get();
                oss << " GROUP BY t.id HAVING COUNT(*) = " << params.clusters.size() << ")";

                query.where(oss.str());
            }

            for (std::string_view keyword : params.keywords)
                query.where("t.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + utils::escapeLikeKeyword(keyword) + "%");

            return query;
        }
    } // namespace

    Listen::Listen(ObjectPtr<User> user, ObjectPtr<Track> track, ScrobblingBackend backend, const Wt::WDateTime& dateTime)
        : _dateTime{ Wt::WDateTime::fromTime_t(dateTime.toTime_t()) }
        , _backend{ backend }
        , _user{ getDboPtr(user) }
        , _track{ getDboPtr(track) }
    {
    }

    Listen::pointer Listen::create(Session& session, ObjectPtr<User> user, ObjectPtr<Track> track, ScrobblingBackend backend, const Wt::WDateTime& dateTime)
    {
        session.checkWriteTransaction();
        return session.getDboSession()->add(std::unique_ptr<Listen>{ new Listen{ user, track, backend, dateTime } });
    }

    std::size_t Listen::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM listen"));
    }

    Listen::pointer Listen::find(Session& session, ListenId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Listen>>("SELECT l from listen l").where("l.id = ?").bind(id));
    }

    RangeResults<ListenId> Listen::find(Session& session, const FindParameters& parameters)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ListenId>("SELECT id FROM listen").orderBy("date_time") };

        if (parameters.user.isValid())
            query.where("user_id = ?").bind(parameters.user);

        if (parameters.backend)
            query.where("backend = ?").bind(*parameters.backend);

        if (parameters.syncState)
            query.where("sync_state = ?").bind(*parameters.syncState);

        return utils::execRangeQuery<ListenId>(query, parameters.range);
    }

    Listen::pointer Listen::find(Session& session, UserId userId, TrackId trackId, ScrobblingBackend backend, const Wt::WDateTime& dateTime)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<Listen>().where("user_id = ?").bind(userId).where("track_id = ?").bind(trackId).where("backend = ?").bind(backend).where("date_time = ?").bind(Wt::WDateTime::fromTime_t(dateTime.toTime_t())));
    }

    RangeResults<ArtistId> Listen::getTopArtists(Session& session, const ArtistStatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createArtistsQuery(session, params) };

        auto collection{ query
                             .orderBy("COUNT(a.id) DESC")
                             .groupBy("a.id") };

        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    RangeResults<ReleaseId> Listen::getTopReleases(Session& session, const StatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createReleasesQuery(session, params)
                        .orderBy("COUNT(r.id) DESC")
                        .groupBy("r.id") };

        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    RangeResults<TrackId> Listen::getTopTracks(Session& session, const StatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createTracksQuery(session, params)
                        .orderBy("COUNT(t.id) DESC")
                        .groupBy("t.id") };

        return utils::execRangeQuery<TrackId>(query, params.range);
    }

    RangeResults<ArtistId> Listen::getRecentArtists(Session& session, const ArtistStatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createArtistsQuery(session, params)
                        .groupBy("a.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    RangeResults<ReleaseId> Listen::getRecentReleases(Session& session, const StatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createReleasesQuery(session, params)
                        .groupBy("r.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    RangeResults<TrackId> Listen::getRecentTracks(Session& session, const StatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createTracksQuery(session, params)
                        .groupBy("t.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        return utils::execRangeQuery<TrackId>(query, params.range);
    }

    std::size_t Listen::getCount(Session& session, UserId userId, TrackId trackId)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) from listen l").join("user u ON u.id = l.user_id").where("l.track_id = ?").bind(trackId).where("l.user_id = ?").bind(userId).where("l.backend = u.scrobbling_backend"));
    }

    std::size_t Listen::getCount(Session& session, UserId userId, ReleaseId releaseId)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>(
                                                                        "SELECT IFNULL(MIN(count_result), 0)"
                                                                        " FROM ("
                                                                        " SELECT COUNT(l.track_id) AS count_result"
                                                                        " FROM track t"
                                                                        " LEFT JOIN listen l ON t.id = l.track_id AND l.backend = (SELECT scrobbling_backend FROM user WHERE id = ?) AND l.user_id = ?"
                                                                        " WHERE t.release_id = ?"
                                                                        " GROUP BY t.id)")
                                                 .bind(userId)
                                                 .bind(userId)
                                                 .bind(releaseId));
    }

    Listen::pointer Listen::getMostRecentListen(Session& session, UserId userId, ScrobblingBackend backend, ReleaseId releaseId)
    {
        session.checkReadTransaction();

        // TODO not pending remove?
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Listen>>("SELECT l from listen l").join("track t ON l.track_id = t.id").where("t.release_id = ?").bind(releaseId).where("l.user_id = ?").bind(userId).where("l.backend = ?").bind(backend).orderBy("l.date_time DESC").limit(1));
    }

    Listen::pointer Listen::getMostRecentListen(Session& session, UserId userId, ScrobblingBackend backend, TrackId trackId)
    {
        session.checkReadTransaction();
        // TODO not pending remove?
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Listen>>("SELECT l from listen l").where("l.track_id = ?").bind(trackId).where("l.user_id = ?").bind(userId).where("l.backend = ?").bind(backend).orderBy("l.date_time DESC").limit(1));
    }
} // namespace lms::db
