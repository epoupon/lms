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

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"

#include "JoinSet.hpp"
#include "Utils.hpp"
#include "objects/FilterExprCompiler.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Listen)

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

            // The joins of the filter use the tracks: they must come first
            if (params.filter && requiresTracks(*params.filter, FilterEntity::Artist))
                joins.add("track t ON t.id = t_a_l.track_id");
            res.combineWithAnd(compileFilterExpr(params.filter, FilterEntity::Artist, params.filterUser, joins));

            return res;
        }

        Wt::Dbo::Query<ArtistId> createArtistsQuery(Session& session, const Listen::ArtistStatsFindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createArtistsConditions(params, joins) };

            auto query{ session.getDboSession()->query<ArtistId>("SELECT a.id from artist a") };
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

            res.combineWithAnd(compileFilterExpr(params.filter, FilterEntity::Release, params.filterUser, joins));

            return res;
        }

        Wt::Dbo::Query<ReleaseId> createReleasesQuery(Session& session, const Listen::StatsFindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createReleasesConditions(params, joins) };

            auto query{ session.getDboSession()->query<ReleaseId>("SELECT r.id from release r") };
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

            res.combineWithAnd(compileFilterExpr(params.filter, FilterEntity::Track, params.filterUser, joins));

            return res;
        }

        Wt::Dbo::Query<TrackId> createTracksQuery(Session& session, const Listen::StatsFindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createTracksConditions(params, joins) };

            auto query{ session.getDboSession()->query<TrackId>("SELECT t.id from track t") };
            joins.apply(query);
            applyWhereFragment(query, where);

            return query;
        }
    } // namespace

    Listen::Listen(ObjectPtr<User> user, ObjectPtr<Track> track, const Wt::WDateTime& dateTime)
        : _dateTime{ Wt::WDateTime::fromTime_t(dateTime.toTime_t()) }
        , _user{ getDboPtr(user) }
        , _track{ getDboPtr(track) }
    {
    }

    Listen::pointer Listen::create(Session& session, ObjectPtr<User> user, ObjectPtr<Track> track, const Wt::WDateTime& dateTime)
    {
        session.checkWriteTransaction();
        return session.getDboSession()->add(std::unique_ptr<Listen>{ new Listen{ user, track, dateTime } });
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

    std::vector<ListenId> Listen::find(Session& session, const FindParameters& parameters)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ListenId>("SELECT id FROM listen").orderBy("date_time") };

        if (parameters.user.isValid())
            query.where("user_id = ?").bind(parameters.user);

        return utils::execRangeQuery<ListenId>(query, parameters.range);
    }

    Listen::pointer Listen::find(Session& session, UserId userId, TrackId trackId, const Wt::WDateTime& dateTime)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<Listen>().where("user_id = ?").bind(userId).where("track_id = ?").bind(trackId).where("date_time = ?").bind(Wt::WDateTime::fromTime_t(dateTime.toTime_t())));
    }

    std::vector<ArtistId> Listen::getTopArtists(Session& session, const ArtistStatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createArtistsQuery(session, params).orderBy("COUNT(a.id) DESC").groupBy("a.id") };
        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    std::vector<ReleaseId> Listen::getTopReleases(Session& session, const StatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createReleasesQuery(session, params).orderBy("COUNT(r.id) DESC").groupBy("r.id") };
        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    std::vector<TrackId> Listen::getTopTracks(Session& session, const StatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createTracksQuery(session, params).orderBy("COUNT(t.id) DESC").groupBy("t.id") };
        return utils::execRangeQuery<TrackId>(query, params.range);
    }

    std::vector<ArtistId> Listen::getRecentArtists(Session& session, const ArtistStatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createArtistsQuery(session, params).groupBy("a.id").having("l.date_time = MAX(l.date_time)").orderBy("l.date_time DESC") };
        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    std::vector<ReleaseId> Listen::getRecentReleases(Session& session, const StatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createReleasesQuery(session, params).groupBy("r.id").having("l.date_time = MAX(l.date_time)").orderBy("l.date_time DESC") };
        return utils::execRangeQuery<ReleaseId>(query, params.range);
    }

    std::vector<TrackId> Listen::getRecentTracks(Session& session, const StatsFindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createTracksQuery(session, params).groupBy("t.id").having("l.date_time = MAX(l.date_time)").orderBy("l.date_time DESC") };
        return utils::execRangeQuery<TrackId>(query, params.range);
    }

    void Listen::getTopArtists(Session& session, const ArtistStatsFindParameters& params, const std::function<void(const ObjectPtr<Artist>&)>& func)
    {
        session.checkReadTransaction();
        auto query{ createArtistsQuery(session, params)
                        .orderBy("COUNT(a.id) DESC")
                        .groupBy("a.id") };

        utils::forEachQueryRangeResult(query, params.range, [&](const ArtistId& id) {
            if (const auto artist{ Artist::find(session, id) })
                func(artist);
        });
    }

    void Listen::getTopReleases(Session& session, const StatsFindParameters& params, const std::function<void(const ObjectPtr<Release>&)>& func)
    {
        session.checkReadTransaction();
        auto query{ createReleasesQuery(session, params)
                        .orderBy("COUNT(r.id) DESC")
                        .groupBy("r.id") };

        utils::forEachQueryRangeResult(query, params.range, [&](const ReleaseId& id) {
            if (const auto release{ Release::find(session, id) })
                func(release);
        });
    }

    void Listen::getTopTracks(Session& session, const StatsFindParameters& params, const std::function<void(const ObjectPtr<Track>&)>& func)
    {
        session.checkReadTransaction();
        auto query{ createTracksQuery(session, params)
                        .orderBy("COUNT(t.id) DESC")
                        .groupBy("t.id") };

        utils::forEachQueryRangeResult(query, params.range, [&](const TrackId& id) {
            if (const auto track{ Track::find(session, id) })
                func(track);
        });
    }

    void Listen::getRecentArtists(Session& session, const ArtistStatsFindParameters& params, const std::function<void(const ObjectPtr<Artist>&)>& func)
    {
        session.checkReadTransaction();
        auto query{ createArtistsQuery(session, params)
                        .groupBy("a.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        utils::forEachQueryRangeResult(query, params.range, [&](const ArtistId& id) {
            if (const auto artist{ Artist::find(session, id) })
                func(artist);
        });
    }

    void Listen::getRecentReleases(Session& session, const StatsFindParameters& params, const std::function<void(const ObjectPtr<Release>&)>& func)
    {
        session.checkReadTransaction();
        auto query{ createReleasesQuery(session, params)
                        .groupBy("r.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        utils::forEachQueryRangeResult(query, params.range, [&](const ReleaseId& id) {
            if (const auto release{ Release::find(session, id) })
                func(release);
        });
    }

    void Listen::getRecentTracks(Session& session, const StatsFindParameters& params, const std::function<void(const ObjectPtr<Track>&)>& func)
    {
        session.checkReadTransaction();
        auto query{ createTracksQuery(session, params)
                        .groupBy("t.id")
                        .having("l.date_time = MAX(l.date_time)")
                        .orderBy("l.date_time DESC") };

        utils::forEachQueryRangeResult(query, params.range, [&](const TrackId& id) {
            if (const auto track{ Track::find(session, id) })
                func(track);
        });
    }

    std::size_t Listen::getCount(Session& session, UserId userId, TrackId trackId)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) from listen l").where("l.track_id = ?").bind(trackId).where("l.user_id = ?").bind(userId));
    }

    std::size_t Listen::getCount(Session& session, UserId userId, ReleaseId releaseId)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>(
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

    Listen::pointer Listen::getMostRecentListen(Session& session, UserId userId, ReleaseId releaseId)
    {
        session.checkReadTransaction();

        // clang-format off
        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Listen>>("SELECT l from listen l")
                        .join("track t ON l.track_id = t.id")
                        .where("t.release_id = ?").bind(releaseId)
                        .where("l.user_id = ?").bind(userId)
                        .orderBy("l.date_time DESC")
                        .limit(1)
        };
        // clang-format on

        return utils::fetchQuerySingleResult(query);
    }

    Listen::pointer Listen::getMostRecentListen(Session& session, UserId userId, TrackId trackId)
    {
        session.checkReadTransaction();

        // clang-format off
        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Listen>>("SELECT l from listen l")
                        .where("l.track_id = ?").bind(trackId)
                        .where("l.user_id = ?").bind(userId)
                        .orderBy("l.date_time DESC")
                        .limit(1)
        };
        // clang-format on

        return utils::fetchQuerySingleResult(query);
    }
} // namespace lms::db
