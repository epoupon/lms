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

#include "database/objects/Listen.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "ArtistDbo.hpp"
#include "JoinSet.hpp"
#include "ListenDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "TrackDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "objects/FilterExprCompiler.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Listen)

namespace lms::db
{
    namespace
    {
        // The listens of an artist are the ones of the tracks it is track artist of
        SqlFragment createArtistsConditions(const Listen::ArtistStatsFindParameters& params, JoinSet& joins)
        {
            assert(!params.artist.isValid()); // poor check

            joins.add("track_artist_link t_a_l ON t_a_l.artist_id = a.id");
            joins.add("listen l ON l.track_id = t_a_l.track_id");

            SqlFragment res;

            if (params.user.isValid())
                res.combineWithAnd({ "l.user_id = ?", { params.user.getValue() } });

            if (params.trackArtistLinkType.has_value())
                res.combineWithAnd({ "t_a_l.type = ?", { static_cast<int>(*params.trackArtistLinkType) } });

            if (params.releaseArtistsOnly)
                joins.add("release_artist_link r_a_l ON r_a_l.artist_id = a.id");

            if (!params.keywords.empty())
            {
                const auto createKeywordClauses{ [&](std::string_view column) {
                    SqlFragment clauses;
                    for (const std::string_view keyword : params.keywords)
                    {
                        if (!clauses.isEmpty())
                            clauses.append(" AND ");
                        clauses.append(std::string{ column } + " LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'", { "%" + utils::escapeForLikeKeyword(keyword) + "%" });
                    }
                    return clauses;
                } };

                SqlFragment keywords{ "(" };
                keywords.append(createKeywordClauses("a.name"));
                keywords.append(") OR (");
                keywords.append(createKeywordClauses("a.sort_name"));
                keywords.append(")");
                res.combineWithAnd(keywords);
            }

            if (params.filter)
            {
                joins.add("track t ON t.id = t_a_l.track_id");
                res.combineWithAnd(compileFilterExpr(*params.filter, params.filterUser));
            }

            return res;
        }

        Wt::Dbo::Query<ArtistId> createArtistsQuery(ISession& session, const Listen::ArtistStatsFindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createArtistsConditions(params, joins) };

            auto query{ getDboSession(session)->query<ArtistId>("SELECT a.id from artist a") };
            joins.apply(query);
            applyWhereFragment(query, where);

            return query;
        }

        SqlFragment createReleasesConditions(const Listen::StatsFindParameters& params, JoinSet& joins)
        {
            joins.add("track t ON t.release_id = r.id");
            joins.add("listen l ON l.track_id = t.id");

            SqlFragment res;

            if (params.user.isValid())
                res.combineWithAnd({ "l.user_id = ?", { params.user.getValue() } });

            if (params.artist.isValid())
            {
                joins.add("track_artist_link t_a_l ON t_a_l.track_id = t.id");
                res.combineWithAnd({ "t_a_l.artist_id = ?", { params.artist.getValue() } });
            }

            for (const std::string_view keyword : params.keywords)
                res.combineWithAnd({ "r.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'", { "%" + utils::escapeForLikeKeyword(keyword) + "%" } });

            if (params.filter)
                res.combineWithAnd(compileFilterExpr(*params.filter, params.filterUser));

            return res;
        }

        Wt::Dbo::Query<ReleaseId> createReleasesQuery(ISession& session, const Listen::StatsFindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createReleasesConditions(params, joins) };

            auto query{ getDboSession(session)->query<ReleaseId>("SELECT r.id from release r") };
            joins.apply(query);
            applyWhereFragment(query, where);

            return query;
        }

        SqlFragment createTracksConditions(const Listen::StatsFindParameters& params, JoinSet& joins)
        {
            joins.add("listen l ON l.track_id = t.id");

            SqlFragment res;

            if (params.user.isValid())
                res.combineWithAnd({ "l.user_id = ?", { params.user.getValue() } });

            if (params.artist.isValid())
            {
                joins.add("track_artist_link t_a_l ON t_a_l.track_id = t.id");
                res.combineWithAnd({ "t_a_l.artist_id = ?", { params.artist.getValue() } });
            }

            for (const std::string_view keyword : params.keywords)
                res.combineWithAnd({ "t.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'", { "%" + utils::escapeForLikeKeyword(keyword) + "%" } });

            if (params.filter)
                res.combineWithAnd(compileFilterExpr(*params.filter, params.filterUser));

            return res;
        }

        Wt::Dbo::Query<TrackId> createTracksQuery(ISession& session, const Listen::StatsFindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createTracksConditions(params, joins) };

            auto query{ getDboSession(session)->query<TrackId>("SELECT t.id from track t") };
            joins.apply(query);
            applyWhereFragment(query, where);

            return query;
        }
    } // namespace

    Listen::Listen(const Wt::WDateTime& dateTime)
        : _dateTime{ Wt::WDateTime::fromTime_t(dateTime.toTime_t()) }
    {
    }

    dbo::Listen::Listen(const ObjectPtr<db::User>& user, const ObjectPtr<db::Track>& track, const Wt::WDateTime& dateTime)
        : db::Listen{ dateTime }
        , _user{ getDboPtr(user) }
        , _track{ getDboPtr(track) }
    {
    }

    Listen::pointer Listen::create(ISession& session, const ObjectPtr<User>& user, const ObjectPtr<Track>& track, const Wt::WDateTime& dateTime)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Listen>{ new dbo::Listen{ user, track, dateTime } });
    }

    std::size_t Listen::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM listen"));
    }

    Listen::pointer Listen::find(ISession& session, ListenId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<dbo::Listen>>("SELECT l from listen l").where("l.id = ?").bind(id));
    }

    std::vector<ListenId> Listen::find(ISession& session, const FindParameters& parameters)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<ListenId>("SELECT id FROM listen").orderBy("date_time") };

        if (parameters.user.isValid())
            query.where("user_id = ?").bind(parameters.user);

        return utils::execRangeQuery<ListenId>(query, parameters.range);
    }

    Listen::pointer Listen::find(ISession& session, UserId userId, TrackId trackId, const Wt::WDateTime& dateTime)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::Listen>().where("user_id = ?").bind(userId).where("track_id = ?").bind(trackId).where("date_time = ?").bind(Wt::WDateTime::fromTime_t(dateTime.toTime_t())));
    }

    std::vector<ArtistId> Listen::getTopArtists(ISession& session, const ArtistStatsFindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createArtistsQuery(session, params).orderBy("COUNT(a.id) DESC").groupBy("a.id") };
        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    std::vector<ReleaseId> Listen::getTopReleases(ISession& session, const StatsFindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createReleasesQuery(session, params).orderBy("COUNT(r.id) DESC").groupBy("r.id") };
        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    std::vector<TrackId> Listen::getTopTracks(ISession& session, const StatsFindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createTracksQuery(session, params).orderBy("COUNT(t.id) DESC").groupBy("t.id") };
        return utils::execRangeQuery<TrackId>(query, params.range);
    }

    std::vector<ArtistId> Listen::getRecentArtists(ISession& session, const ArtistStatsFindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createArtistsQuery(session, params).groupBy("a.id").having("l.date_time = MAX(l.date_time)").orderBy("l.date_time DESC") };
        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    std::vector<ReleaseId> Listen::getRecentReleases(ISession& session, const StatsFindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createReleasesQuery(session, params).groupBy("r.id").having("l.date_time = MAX(l.date_time)").orderBy("l.date_time DESC") };
        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    std::vector<TrackId> Listen::getRecentTracks(ISession& session, const StatsFindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createTracksQuery(session, params).groupBy("t.id").having("l.date_time = MAX(l.date_time)").orderBy("l.date_time DESC") };
        return utils::execRangeQuery<TrackId>(query, params.range);
    }

    void Listen::getTopArtists(ISession& session, const ArtistStatsFindParameters& params, const std::function<void(const ObjectPtr<Artist>&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createArtistsQuery(session, params)
                        .orderBy("COUNT(a.id) DESC")
                        .groupBy("a.id") };

        utils::forEachQueryRangeResult(query, params.range, [&](const ArtistId& id) {
            if (const auto artist{ Artist::find(session, id) })
                func(artist);
        });
    }

    void Listen::getTopReleases(ISession& session, const StatsFindParameters& params, const std::function<void(const ObjectPtr<Release>&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createReleasesQuery(session, params)
                        .orderBy("COUNT(r.id) DESC")
                        .groupBy("r.id") };

        utils::forEachQueryRangeResult(query, params.range, [&](const ReleaseId& id) {
            if (const auto release{ Release::find(session, id) })
                func(release);
        });
    }

    void Listen::getTopTracks(ISession& session, const StatsFindParameters& params, const std::function<void(const ObjectPtr<Track>&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createTracksQuery(session, params)
                        .orderBy("COUNT(t.id) DESC")
                        .groupBy("t.id") };

        utils::forEachQueryRangeResult(query, params.range, [&](const TrackId& id) {
            if (const auto track{ Track::find(session, id) })
                func(track);
        });
    }

    void Listen::getRecentArtists(ISession& session, const ArtistStatsFindParameters& params, const std::function<void(const ObjectPtr<Artist>&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createArtistsQuery(session, params)
                        .groupBy("a.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        utils::forEachQueryRangeResult(query, params.range, [&](const ArtistId& id) {
            if (const auto artist{ Artist::find(session, id) })
                func(artist);
        });
    }

    void Listen::getRecentReleases(ISession& session, const StatsFindParameters& params, const std::function<void(const ObjectPtr<Release>&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createReleasesQuery(session, params)
                        .groupBy("r.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        utils::forEachQueryRangeResult(query, params.range, [&](const ReleaseId& id) {
            if (const auto release{ Release::find(session, id) })
                func(release);
        });
    }

    void Listen::getRecentTracks(ISession& session, const StatsFindParameters& params, const std::function<void(const ObjectPtr<Track>&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createTracksQuery(session, params)
                        .groupBy("t.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        utils::forEachQueryRangeResult(query, params.range, [&](const TrackId& id) {
            if (const auto track{ Track::find(session, id) })
                func(track);
        });
    }

    std::size_t Listen::getCount(ISession& session, UserId userId, TrackId trackId)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) from listen l").where("l.track_id = ?").bind(trackId).where("l.user_id = ?").bind(userId));
    }

    std::size_t Listen::getCount(ISession& session, UserId userId, ReleaseId releaseId)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>(
                                                                       "SELECT IFNULL(MIN(count_result), 0)"
                                                                       " FROM ("
                                                                       " SELECT COUNT(l.track_id) AS count_result"
                                                                       " FROM track t"
                                                                       " LEFT JOIN listen l ON t.id = l.track_id AND l.user_id = ?"
                                                                       " WHERE t.release_id = ?"
                                                                       " GROUP BY t.id)")
                                                 .bind(userId)
                                                 .bind(releaseId));
    }

    Listen::pointer Listen::getMostRecentListen(ISession& session, UserId userId, ReleaseId releaseId)
    {
        checkReadTransaction(session);

        // clang-format off
        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Listen>>("SELECT l from listen l")
                        .join("track t ON l.track_id = t.id")
                        .where("t.release_id = ?").bind(releaseId)
                        .where("l.user_id = ?").bind(userId)
                        .orderBy("l.date_time DESC")
                        .limit(1)
        };
        // clang-format on

        return utils::fetchQuerySingleResult(query);
    }

    Listen::pointer Listen::getMostRecentListen(ISession& session, UserId userId, TrackId trackId)
    {
        checkReadTransaction(session);

        // clang-format off
        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::Listen>>("SELECT l from listen l")
                        .where("l.track_id = ?").bind(trackId)
                        .where("l.user_id = ?").bind(userId)
                        .orderBy("l.date_time DESC")
                        .limit(1)
        };
        // clang-format on

        return utils::fetchQuerySingleResult(query);
    }

    ObjectPtr<User> Listen::getUser() const
    {
        return getDbo(*this)._user;
    }

    ObjectPtr<Track> Listen::getTrack() const
    {
        return getDbo(*this)._track;
    }
} // namespace lms::db
