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
#include "database/objects/Artist.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"

#include "JoinSet.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "objects/FilterExprCompiler.hpp"
#include "objects/detail/Types.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"
#include "traits/UUIDTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Artist)

namespace lms::db
{
    namespace
    {
        // Several conditions can need the same join, which is then added once
        constexpr std::string_view trackArtistLinkJoin{ "track_artist_link t_a_l ON t_a_l.artist_id = a.id" };
        constexpr std::string_view releaseArtistLinkJoin{ "release_artist_link r_a_l ON r_a_l.artist_id = a.id" };
        constexpr std::string_view artistFeedbackJoin{ "artist_feedback a_f ON a_f.artist_id = a.id" };

        void addTrackArtistJoins(JoinSet& joins)
        {
            joins.add(trackArtistLinkJoin);
            joins.add("track t ON t.id = t_a_l.track_id");
        }

        void addReleaseArtistJoins(JoinSet& joins)
        {
            joins.add(releaseArtistLinkJoin);
            joins.add("track t ON t.release_id = r_a_l.release_id");
        }

        // The tracks of an artist are the ones it is track artist of, or the ones of its releases if it is release artist
        void addTracksJoins(JoinSet& joins, const Artist::FindParameters& params)
        {
            if (params.releaseArtistsOnly)
                addReleaseArtistJoins(joins);
            else
                addTrackArtistJoins(joins);
        }

        bool needsTracks(const Artist::FindParameters& params)
        {
            return params.sortMethod == ArtistSortMethod::LastWrittenDesc || params.sortMethod == ArtistSortMethod::AddedDesc;
        }

        // The artists having a matching track, through the links that put them in the list
        SqlFragment createFilterCondition(const Artist::FindParameters& params)
        {
            SqlFragment res{ "a.id IN (" };
            if (!params.releaseArtistsOnly)
            {
                res.append("SELECT t_a_l.artist_id FROM track_artist_link t_a_l WHERE ");
                res.append(createMatchingTracksCondition("t_a_l.track_id", *params.filter, params.filterUser));
                if (params.trackArtistLinkType)
                    res.append(" AND t_a_l.type = ?", { static_cast<int>(*params.trackArtistLinkType) });
            }
            if (!params.trackArtistLinkType)
            {
                // a membership test needs no deduplication
                if (!params.releaseArtistsOnly)
                    res.append(" UNION ALL ");
                res.append("SELECT r_a_l.artist_id FROM release_artist_link r_a_l WHERE ");
                res.append(createMatchingTracksCondition("r_a_l.release_id", *params.filter, params.filterUser, "t.release_id"));
            }
            res.append(")");

            return res;
        }

        SqlFragment createKeywordsCondition(const std::vector<std::string_view>& keywords)
        {
            const auto createKeywordClauses{ [&](std::string_view column) {
                SqlFragment res;
                for (const std::string_view keyword : keywords)
                {
                    if (!res.isEmpty())
                        res.append(" AND ");
                    res.append(std::string{ column } + " LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'", { "%" + utils::escapeForLikeKeyword(keyword) + "%" });
                }
                return res;
            } };

            SqlFragment res{ "(" };
            res.append(createKeywordClauses("a.name"));
            res.append(") OR (");
            res.append(createKeywordClauses("a.sort_name"));
            res.append(")");
            return res;
        }

        // Returns the condition of the parameters, and adds to joins the joins it relies on. The filter is included only if the tracks are joined
        SqlFragment createParamsConditions(const Artist::FindParameters& params, JoinSet& joins)
        {
            SqlFragment res;

            if (params.releaseArtistsOnly)
                joins.add(releaseArtistLinkJoin);

            if (params.trackArtistLinkType.has_value())
            {
                joins.add(trackArtistLinkJoin);
                res.combineWithAnd({ "t_a_l.type = ?", { static_cast<int>(*params.trackArtistLinkType) } });
            }

            if (!params.keywords.empty())
                res.combineWithAnd(createKeywordsCondition(params.keywords));

            if (params.feedbackUser.isValid())
            {
                joins.add(artistFeedbackJoin);
                res.combineWithAnd({ "a_f.user_id = ?", { params.feedbackUser.getValue() } });
            }
            if (params.feedbackValue)
            {
                joins.add(artistFeedbackJoin);
                res.combineWithAnd({ "a_f.value = ?", { static_cast<int>(*params.feedbackValue) } });
            }

            if (params.track.isValid())
            {
                joins.add(trackArtistLinkJoin);
                res.combineWithAnd({ "t_a_l.track_id = ?", { params.track.getValue() } });
            }

            // The joins of a sort select the artists too: the ones without any row are excluded
            if (needsTracks(params))
            {
                addTracksJoins(joins, params);
                // the date only considers the matching tracks
                if (params.filter)
                    res.combineWithAnd(compileFilterExpr(*params.filter, params.filterUser));
            }
            if (params.sortMethod == ArtistSortMethod::FeedbackDateDesc)
                joins.add(artistFeedbackJoin);

            return res;
        }

        // A sort on a date is the aggregate of a column over all the joined rows of an artist.
        // With the union, each branch computes it as "sort_key", and it is aggregated again over both branches as an artist can be in both
        struct SortDate
        {
            std::string_view function;
            std::string_view column;
        };

        std::optional<SortDate> getSortDate(ArtistSortMethod sortMethod)
        {
            switch (sortMethod)
            {
            case ArtistSortMethod::LastWrittenDesc:
                return SortDate{ "MAX", "t.file_last_write" };
            case ArtistSortMethod::AddedDesc:
                return SortDate{ "MIN", "t.file_added" };
            case ArtistSortMethod::FeedbackDateDesc:
                return SortDate{ "MAX", "a_f.date_time" };
            case ArtistSortMethod::None:
            case ArtistSortMethod::Id:
            case ArtistSortMethod::Name:
            case ArtistSortMethod::SortName:
            case ArtistSortMethod::Random:
                return std::nullopt;
            }

            throw Exception{ "Unhandled artist sort method" };
        }

        std::string createSortDateExpr(ArtistSortMethod sortMethod, bool useUnion)
        {
            const std::optional<SortDate> sortDate{ getSortDate(sortMethod) };
            assert(sortDate);

            return std::string{ sortDate->function } + "(" + (useUnion ? "f_u.sort_key" : std::string{ sortDate->column }) + ")";
        }

        // The tracks of an artist are the ones it is track artist of and the ones of the releases it is release artist of,
        // unless the parameters already restrict the artists to one of them
        bool usesUnion(const Artist::FindParameters& params)
        {
            return needsTracks(params) && !params.releaseArtistsOnly && !params.trackArtistLinkType && !params.track.isValid();
        }

        // A complete artist query, so that each branch keeps its own join order
        SqlFragment createUnionBranch(const Artist::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createParamsConditions(params, joins) };

            const std::optional<SortDate> sortDate{ getSortDate(params.sortMethod) };
            assert(sortDate);

            SqlFragment res{ "SELECT a.id AS artist_id, " + std::string{ sortDate->function } + "(" + std::string{ sortDate->column } + ") AS sort_key FROM artist a" };
            res.append(joins.toSql());
            if (!where.isEmpty())
            {
                res.append(" WHERE ");
                res.append(where);
            }
            res.append(" GROUP BY a.id");

            return res;
        }

        // The artists that are track artists, and the ones that are release artists
        SqlFragment createUnionJoin(const Artist::FindParameters& params)
        {
            assert(!params.releaseArtistsOnly);

            Artist::FindParameters releaseArtistParams{ params };
            releaseArtistParams.releaseArtistsOnly = true;

            SqlFragment res{ "(" };
            res.append(createUnionBranch(params));
            res.append(" UNION ALL ");
            res.append(createUnionBranch(releaseArtistParams));
            res.append(") f_u ON f_u.artist_id = a.id");

            return res;
        }

        // Returns the condition selecting the artists matching the parameters, and adds to joins the joins it relies on
        SqlFragment createArtistSelection(const Artist::FindParameters& params, JoinSet& joins)
        {
            // A link type needs the track artist links, and releaseArtistsOnly the release artist ones: they would not describe the same tracks
            assert(!params.releaseArtistsOnly || !params.trackArtistLinkType.has_value());

            // The conditions are inside each branch of the union
            SqlFragment res;
            if (usesUnion(params))
                joins.add(createUnionJoin(params));
            else
                res = createParamsConditions(params, joins);

            if (params.filter && !needsTracks(params))
                res.combineWithAnd(createFilterCondition(params));

            return res;
        }

        template<typename ResultType>
        void applySortMethod(Wt::Dbo::Query<ResultType>& query, const Artist::FindParameters& params)
        {
            const bool useUnion{ usesUnion(params) };

            switch (params.sortMethod)
            {
            case ArtistSortMethod::None:
                break;
            case ArtistSortMethod::Id:
                query.orderBy("a.id");
                break;
            case ArtistSortMethod::Name:
                query.orderBy("a.name COLLATE NOCASE");
                break;
            case ArtistSortMethod::SortName:
                query.orderBy("COALESCE(NULLIF(a.sort_name, ''), a.name) COLLATE NOCASE");
                break;
            case ArtistSortMethod::Random:
                query.orderBy("RANDOM()");
                break;
            case ArtistSortMethod::LastWrittenDesc:
            case ArtistSortMethod::AddedDesc:
                query.orderBy(createSortDateExpr(params.sortMethod, useUnion) + " DESC, COALESCE(NULLIF(a.sort_name, ''), a.name) COLLATE NOCASE");
                break;
            case ArtistSortMethod::FeedbackDateDesc:
                assert(params.feedbackUser.isValid());
                query.orderBy(createSortDateExpr(params.sortMethod, useUnion) + " DESC");
                break;
            }
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createListQuery(ISession& session, std::string_view selectedItem, const Artist::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createArtistSelection(params, joins) };

            auto query{ getDboSession(session)->query<ResultType>("SELECT " + std::string{ selectedItem } + " FROM artist a") };
            joins.apply(query);
            applyWhereFragment(query, where);
            applySortMethod(query, params);

            // Joins can duplicate artists
            if (!joins.empty())
                query.groupBy("a.id");

            return query;
        }

        Wt::Dbo::Query<int> createCountQuery(ISession& session, const Artist::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createArtistSelection(params, joins) };

            // Joins can duplicate artists
            auto query{ getDboSession(session)->query<int>(joins.empty() ? "SELECT COUNT(*) FROM artist a" : "SELECT COUNT(DISTINCT a.id) FROM artist a") };
            joins.apply(query);
            applyWhereFragment(query, where);

            return query;
        }
    } // namespace

    Artist::Artist(const std::string& name, const std::optional<core::UUID>& mbid)
        : _mbid{ mbid }
    {
        setName(name);
    }

    Artist::pointer Artist::create(ISession& session, const std::string& name, const std::optional<core::UUID>& mbid)
    {
        return utils::addObject(session, std::unique_ptr<Artist>{ new Artist{ name, mbid } });
    }

    void Artist::destroy(ISession& session, std::span<const ArtistId> ids)
    {
        utils::destroyObjects<Artist>(session, ids);
    }

    std::size_t Artist::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM artist"));
    }

    std::size_t Artist::getCount(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(createCountQuery(session, params));
    }

    void Artist::find(ISession& session, ArtistId& lastRetrievedArtist, std::size_t count, const std::function<void(const Artist::pointer&)>& func, MediaLibraryId library)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").orderBy("a.id").where("a.id > ?").bind(lastRetrievedArtist).limit(static_cast<int>(count)) };

        if (library.isValid())
        {
            // Faster than using joins
            query.where(
                     "EXISTS (SELECT 1 FROM track_artist_link t_a_l JOIN track t ON t.id = t_a_l.track_id WHERE t_a_l.artist_id = a.id AND t.media_library_id = ?)"
                     " OR EXISTS (SELECT 1 FROM release_artist_link r_a_l JOIN release r ON r.id = r_a_l.release_id JOIN track t ON t.release_id = r.id WHERE r_a_l.artist_id = a.id AND t.media_library_id = ?)")
                .bind(library)
                .bind(library);
        }

        utils::forEachQueryResult(query, [&](const Artist::pointer& artist) {
            func(artist);
            lastRetrievedArtist = artist->getId();
        });
    }

    void Artist::find(ISession& session, const IdRange<ArtistId>& idRange, const std::function<void(const Artist::pointer&)>& func)
    {
        checkReadTransaction(session);

        assert(idRange.isValid());

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<Artist>>("SELECT a from artist a").orderBy("a.id").where("a.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryResult(query, [&](const Artist::pointer& artist) {
            func(artist);
        });
    }

    std::vector<Artist::pointer> Artist::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        name = core::stringUtils::utf8Truncate(name, maxNameLength);

        return utils::fetchQueryResults<Artist::pointer>(getDboSession(session)->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.name = ?").bind(name).orderBy("LENGTH(a.mbid) DESC")); // put mbid entries first
    }

    Artist::pointer Artist::find(ISession& session, const core::UUID& mbid)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.mbid = ?").bind(mbid));
    }

    Artist::pointer Artist::find(ISession& session, ArtistId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.id = ?").bind(id));
    }

    std::vector<ArtistId> Artist::findIds(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<ArtistId>(session, "a.id", params) };
        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    std::vector<Artist::pointer> Artist::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<Artist>>(session, "a", params) };
        return utils::execRangeQuery<Artist::pointer>(query, params.range);
    }

    void Artist::find(ISession& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        checkReadTransaction(session);

        auto query{ createListQuery<Wt::Dbo::ptr<Artist>>(session, "a", params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    IdRange<ArtistId> Artist::findNextIdRange(ISession& session, ArtistId lastRetrievedId, std::size_t count)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<std::tuple<ArtistId, ArtistId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT a.id FROM artist a WHERE a.id > ? ORDER BY a.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<ArtistId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    std::vector<ArtistId> Artist::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        // TODO extend with release artists
        checkReadTransaction(session);
        auto query{ getDboSession(session)->query<ArtistId>(R"(SELECT DISTINCT a.id FROM artist a 
WHERE NOT EXISTS (
    SELECT 1 
    FROM track t 
    INNER JOIN track_artist_link t_a_l 
    ON t_a_l.artist_id = a.id 
    WHERE t.id = t_a_l.track_id
)
AND NOT EXISTS (
    SELECT 1 
    FROM release r 
    INNER JOIN release_artist_link r_a_l 
    ON r_a_l.artist_id = a.id 
    WHERE r.id = r_a_l.release_id)
AND NOT EXISTS (
    SELECT 1 
    FROM artist_info ai 
    WHERE ai.artist_id = a.id))") };
        return utils::execRangeQuery<ArtistId>(query, range);
    }

    bool Artist::exists(ISession& session, ArtistId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT 1 FROM artist").where("id = ?").bind(id)) == 1;
    }

    void Artist::findNameUpdates(ISession& session, const std::function<void(const pointer& artist, std::string_view newName, std::string_view newSortName)>& func)
    {
        checkReadTransaction(session);

        // Source priority: artist info files, MBID-matched release then track links, other release then track links; then the most recent, then the last created
        // Only artists with an MBID get renamed: the others are identified by their name
        // Correlated subqueries served by the artist_id/release_id indexes: automatic indexes are disabled, joins on derived tables would be nested scans
        const std::string sources{ R"(
            SELECT a_i.name AS name, a_i.sort_name AS sort_name, CASE WHEN a_i.mbid_matched THEN 0 ELSE 1 END AS priority, NULL AS date, a_i.id AS id
            FROM artist_info a_i WHERE a_i.artist_id = a_n.id
            UNION ALL
            SELECT r_a_l.artist_name, r_a_l.artist_sort_name, CASE WHEN r_a_l.artist_mbid_matched THEN 2 ELSE 4 END, (SELECT MAX(COALESCE(t.original_date, t.date)) FROM track t WHERE t.release_id = r_a_l.release_id), r_a_l.id
            FROM release_artist_link r_a_l WHERE r_a_l.artist_id = a_n.id
            UNION ALL
            SELECT t_a_l.artist_name, t_a_l.artist_sort_name, CASE WHEN t_a_l.artist_mbid_matched THEN 3 ELSE 5 END, COALESCE(t.original_date, t.date), t_a_l.id
            FROM track_artist_link t_a_l JOIN track t ON t.id = t_a_l.track_id WHERE t_a_l.artist_id = a_n.id)" };
        const std::string bestSourceFirst{ " ORDER BY s.priority, s.date DESC, s.id DESC LIMIT 1" };

        auto query{ getDboSession(session)->query<std::tuple<Wt::Dbo::ptr<Artist>, std::string, std::string>>(
            "SELECT a, n.name, n.sort_name FROM ("
            " SELECT a_n.id AS artist_id,"
            " CASE WHEN a_n.mbid IS NULL THEN a_n.name ELSE COALESCE((SELECT s.name FROM ("
            + sources + ") s" + bestSourceFirst + "), a_n.name) END AS name,"
                                                  " COALESCE((SELECT s.sort_name FROM ("
            + sources + ") s WHERE s.sort_name <> ''" + bestSourceFirst + "), '') AS sort_name"
                                                                          " FROM artist a_n) n"
                                                                          " JOIN artist a ON a.id = n.artist_id"
                                                                          " WHERE a.name <> n.name OR a.sort_name <> n.sort_name") };

        utils::forEachQueryResult(query, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res), std::get<2>(res));
        });
    }

    void Artist::updatePreferredArtwork(ISession& session, ArtistId artistId, ArtworkId artworkId)
    {
        checkWriteTransaction(session);

        if (artworkId.isValid())
            utils::executeCommand(*getDboSession(session), "UPDATE artist SET preferred_artwork_id = ? WHERE id = ?", artworkId, artistId);
        else
            utils::executeCommand(*getDboSession(session), "UPDATE artist SET preferred_artwork_id = NULL WHERE id = ?", artistId);
    }

    ObjectPtr<Artwork> Artist::getPreferredArtwork() const
    {
        return ObjectPtr<Artwork>{ _preferredArtwork };
    }

    ArtworkId Artist::getPreferredArtworkId() const
    {
        return _preferredArtwork.id();
    }

    void Artist::setName(std::string_view name)
    {
        _name = core::stringUtils::utf8Truncate(name, maxNameLength);
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Artist name too long, truncated to '" << _name << "'");
    }

    void Artist::setSortName(std::string_view sortName)
    {
        _sortName = core::stringUtils::utf8Truncate(sortName, maxNameLength);
        LMS_LOG_IF(DB, WARNING, sortName.size() > maxNameLength, "Artist sort name too long, truncated to '" << _sortName << "'");
    }

    void Artist::setPreferredArtwork(const ObjectPtr<Artwork>& artwork)
    {
        _preferredArtwork = getDboPtr(artwork);
    }
} // namespace lms::db
