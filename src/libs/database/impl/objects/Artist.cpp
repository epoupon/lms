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

#include "database/Session.hpp"
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

        // Returns the condition of the parameters, filter included, and adds to joins the joins it relies on
        SqlFragment createParamsConditions(const Artist::FindParameters& params, JoinSet& joins)
        {
            // The joins of the filter use the tracks: they must come first
            if (params.filter && requiresTracks(*params.filter, FilterEntity::Artist))
                addTracksJoins(joins, params);
            SqlFragment res{ compileFilterExpr(params.filter, FilterEntity::Artist, params.filterUser, joins) };

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
            if (params.sortMethod == ArtistSortMethod::LastWrittenDesc || params.sortMethod == ArtistSortMethod::AddedDesc)
                addTracksJoins(joins, params);
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

        bool needsTracks(const Artist::FindParameters& params)
        {
            return params.sortMethod == ArtistSortMethod::LastWrittenDesc
                || params.sortMethod == ArtistSortMethod::AddedDesc
                || (params.filter && requiresTracks(*params.filter, FilterEntity::Artist));
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

            SqlFragment res{ "SELECT a.id AS artist_id, " };
            if (const std::optional<SortDate> sortDate{ getSortDate(params.sortMethod) })
                res.append(std::string{ sortDate->function } + "(" + std::string{ sortDate->column } + ")");
            else
                res.append("NULL");
            res.append(" AS sort_key FROM artist a");
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
            if (usesUnion(params))
            {
                joins.add(createUnionJoin(params));
                return {};
            }

            return createParamsConditions(params, joins);
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
        Wt::Dbo::Query<ResultType> createListQuery(Session& session, std::string_view selectedItem, const Artist::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createArtistSelection(params, joins) };

            auto query{ session.getDboSession()->query<ResultType>("SELECT " + std::string{ selectedItem } + " FROM artist a") };
            joins.apply(query);
            applyWhereFragment(query, where);
            applySortMethod(query, params);

            // Joins can duplicate artists
            if (!joins.empty())
                query.groupBy("a.id");

            return query;
        }

        Wt::Dbo::Query<int> createCountQuery(Session& session, const Artist::FindParameters& params)
        {
            JoinSet joins;
            const SqlFragment where{ createArtistSelection(params, joins) };

            // Joins can duplicate artists
            auto query{ session.getDboSession()->query<int>(joins.empty() ? "SELECT COUNT(*) FROM artist a" : "SELECT COUNT(DISTINCT a.id) FROM artist a") };
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

    Artist::pointer Artist::create(Session& session, const std::string& name, const std::optional<core::UUID>& mbid)
    {
        return session.getDboSession()->add(std::unique_ptr<Artist>{ new Artist{ name, mbid } });
    }

    std::size_t Artist::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM artist"));
    }

    std::size_t Artist::getCount(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(createCountQuery(session, params));
    }

    void Artist::find(Session& session, ArtistId& lastRetrievedArtist, std::size_t count, const std::function<void(const Artist::pointer&)>& func, MediaLibraryId library)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").orderBy("a.id").where("a.id > ?").bind(lastRetrievedArtist).limit(static_cast<int>(count)) };

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

    void Artist::find(Session& session, const IdRange<ArtistId>& idRange, const std::function<void(const Artist::pointer&)>& func)
    {
        assert(idRange.isValid());

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a from artist a").orderBy("a.id").where("a.id BETWEEN ? AND ?").bind(idRange.first).bind(idRange.last) };

        utils::forEachQueryResult(query, [&](const Artist::pointer& artist) {
            func(artist);
        });
    }

    std::vector<Artist::pointer> Artist::find(Session& session, std::string_view name)
    {
        session.checkReadTransaction();

        name = core::stringUtils::utf8Truncate(name, maxNameLength);

        return utils::fetchQueryResults<Artist::pointer>(session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.name = ?").bind(name).orderBy("LENGTH(a.mbid) DESC")); // put mbid entries first
    }

    Artist::pointer Artist::find(Session& session, const core::UUID& mbid)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.mbid = ?").bind(mbid));
    }

    Artist::pointer Artist::find(Session& session, ArtistId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<Artist>>("SELECT a FROM artist a").where("a.id = ?").bind(id));
    }

    std::vector<ArtistId> Artist::findIds(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ createListQuery<ArtistId>(session, "a.id", params) };
        return utils::execRangeQuery<ArtistId>(query, params.range);
    }

    std::vector<Artist::pointer> Artist::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ createListQuery<Wt::Dbo::ptr<Artist>>(session, "a", params) };
        return utils::execRangeQuery<Artist::pointer>(query, params.range);
    }

    void Artist::find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        session.checkReadTransaction();

        auto query{ createListQuery<Wt::Dbo::ptr<Artist>>(session, "a", params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    IdRange<ArtistId> Artist::findNextIdRange(Session& session, ArtistId lastRetrievedId, std::size_t count)
    {
        auto query{ session.getDboSession()->query<std::tuple<ArtistId, ArtistId>>("SELECT MIN(sub.id) AS first_id, MAX(sub.id) AS last_id FROM (SELECT a.id FROM artist a WHERE a.id > ? ORDER BY a.id LIMIT ?) sub") };
        query.bind(lastRetrievedId);
        query.bind(static_cast<int>(count));

        auto res{ utils::fetchQuerySingleResult(query) };
        return IdRange<ArtistId>{ .first = std::get<0>(res), .last = std::get<1>(res) };
    }

    std::vector<ArtistId> Artist::findOrphanIds(Session& session, std::optional<Range> range)
    {
        // TODO extend with release artists
        session.checkReadTransaction();
        auto query{ session.getDboSession()->query<ArtistId>(R"(SELECT DISTINCT a.id FROM artist a 
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

    bool Artist::exists(Session& session, ArtistId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT 1 FROM artist").where("id = ?").bind(id)) == 1;
    }

    std::vector<Artist::pointer> Artist::findWithMBIDMatchedNameOrSortNameVariants(Session& session, ArtistId& lastRetrievedArtist, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Artist>>(R"(
        SELECT a FROM artist a
        WHERE a.id IN (
            SELECT artist_id FROM (
                SELECT artist_id, artist_name, artist_sort_name FROM track_artist_link WHERE artist_mbid_matched = 1
                UNION ALL
                SELECT artist_id, artist_name, artist_sort_name FROM release_artist_link WHERE artist_mbid_matched = 1
            ) links
            GROUP BY artist_id
            HAVING COUNT(DISTINCT artist_name) > 1 OR COUNT(DISTINCT NULLIF(artist_sort_name, '')) > 1
        )
        AND a.id > ?
    )")
                        .bind(lastRetrievedArtist) };

        auto results{ utils::execRangeQuery<Artist::pointer>(query, range) };

        if (!results.empty())
            lastRetrievedArtist = results.back()->getId();

        return results;
    }

    std::vector<Artist::pointer> Artist::findWithNonMBIDSortNameVariants(Session& session, ArtistId& lastRetrievedArtist, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<Artist>>(R"(
        SELECT a FROM artist a
        WHERE a.mbid IS NULL
        AND a.id IN (
            SELECT artist_id FROM (
                SELECT artist_id, artist_sort_name FROM track_artist_link WHERE artist_sort_name <> ''
                UNION ALL
                SELECT artist_id, artist_sort_name FROM release_artist_link WHERE artist_sort_name <> ''
            ) links
            GROUP BY artist_id
            HAVING COUNT(DISTINCT artist_sort_name) > 1
        )
        AND a.id > ?
    )")
                        .bind(lastRetrievedArtist) };

        auto results{ utils::execRangeQuery<Artist::pointer>(query, range) };

        if (!results.empty())
            lastRetrievedArtist = results.back()->getId();

        return results;
    }

    void Artist::updatePreferredArtwork(Session& session, ArtistId artistId, ArtworkId artworkId)
    {
        session.checkWriteTransaction();

        if (artworkId.isValid())
            utils::executeCommand(*session.getDboSession(), "UPDATE artist SET preferred_artwork_id = ? WHERE id = ?", artworkId, artistId);
        else
            utils::executeCommand(*session.getDboSession(), "UPDATE artist SET preferred_artwork_id = NULL WHERE id = ?", artistId);
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

    void Artist::setPreferredArtwork(ObjectPtr<Artwork> artwork)
    {
        _preferredArtwork = getDboPtr(artwork);
    }
} // namespace lms::db
