/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "database/objects/FilterExpr.hpp"

#include "JoinSet.hpp"
#include "SqlFragment.hpp"
#include "objects/FilterExprCompiler.hpp"

namespace lms::db::tests
{
    namespace
    {
        struct Compiled
        {
            JoinSet joins;
            SqlFragment where;
        };

        // Gives the tracks to the queries that need them, as the callers must do
        Compiled compile(const FilterExpr& expr, FilterEntity queriedEntity, UserId user = {})
        {
            Compiled res;
            if (queriedEntity == FilterEntity::Release && requiresTracks(expr, queriedEntity))
                res.joins.add("track t ON t.release_id = r.id");
            else if (queriedEntity == FilterEntity::Artist && requiresTracks(expr, queriedEntity))
                res.joins.add("track t ON t.id = a.id"); // only the alias matters, the SQL is not executed

            res.where = compileFilterExpr(expr, queriedEntity, user, res.joins);
            return res;
        }

        const GenreId genre5{ 5 };
        const GenreId genre7{ 7 };
        const MoodId mood3{ 3 };
        const LabelId label2{ 2 };

        constexpr std::string_view genreMember{ "t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id = ?)" };
        constexpr std::string_view moodMember{ "t.id IN (SELECT f_e.track_id FROM track_mood f_e WHERE f_e.mood_id = ?)" };
    } // namespace

    TEST(FilterExprCompiler, noExpression_isEmpty)
    {
        JoinSet joins;
        const SqlFragment where{ compileFilterExpr(std::nullopt, FilterEntity::Track, UserId{}, joins) };

        EXPECT_TRUE(where.isEmpty());
        EXPECT_TRUE(joins.empty());
    }

    TEST(FilterExprCompiler, relation_isAnInnerJoin)
    {
        const Compiled compiled{ compile(FilterExpr::equal(genre5), FilterEntity::Track) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " JOIN track_genre f_j1 ON f_j1.track_id = t.id");
        EXPECT_EQ(compiled.where.getSql(), "f_j1.genre_id = ?");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL }));
    }

    TEST(FilterExprCompiler, equalRelations_shareTheSameJoin)
    {
        const Compiled compiled{ compile(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::equal(genre5) }), FilterEntity::Track) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " JOIN track_genre f_j1 ON f_j1.track_id = t.id");
        EXPECT_EQ(compiled.where.getSql(), "(f_j1.genre_id = ? AND f_j1.genre_id = ?)");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 5LL }));
    }

    TEST(FilterExprCompiler, differentRelations_getTheirOwnJoin)
    {
        const Compiled compiled{ compile(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::equal(mood3) }), FilterEntity::Track) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " JOIN track_genre f_j1 ON f_j1.track_id = t.id JOIN track_mood f_j2 ON f_j2.track_id = t.id");
        EXPECT_EQ(compiled.where.getSql(), "(f_j1.genre_id = ? AND f_j2.mood_id = ?)");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 3LL }));
    }

    TEST(FilterExprCompiler, relationUnderOr_isAMembershipTest)
    {
        const Compiled compiled{ compile(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(mood3) }), FilterEntity::Track) };

        EXPECT_TRUE(compiled.joins.empty());
        EXPECT_EQ(compiled.where.getSql(), "(" + std::string{ genreMember } + " OR " + std::string{ moodMember } + ")");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 3LL }));
    }

    TEST(FilterExprCompiler, sameRelationsUnderOr_areMergedIntoOneMembershipTest)
    {
        const Compiled compiled{ compile(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(genre7), FilterExpr::equal(GenreId{ 9 }) }), FilterEntity::Track) };

        EXPECT_TRUE(compiled.joins.empty());
        EXPECT_EQ(compiled.where.getSql(), "(t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?,?)))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 9LL }));
    }

    TEST(FilterExprCompiler, mergedRelations_takeThePlaceOfTheFirstOne)
    {
        const Compiled compiled{ compile(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(mood3), FilterExpr::equal(genre7) }), FilterEntity::Track) };

        EXPECT_EQ(compiled.where.getSql(), "(t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?)) OR " + std::string{ moodMember } + ")");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 3LL }));
    }

    TEST(FilterExprCompiler, relationsOfDifferentTypesUnderOr_areMergedPerType)
    {
        const Compiled compiled{ compile(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(mood3), FilterExpr::equal(GenreId{ 9 }), FilterExpr::equal(MoodId{ 4 }) }), FilterEntity::Track) };

        EXPECT_EQ(compiled.where.getSql(), "(t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?)) OR t.id IN (SELECT f_e.track_id FROM track_mood f_e WHERE f_e.mood_id IN (?,?)))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 9LL, 3LL, 4LL }));
    }

    TEST(FilterExprCompiler, mergedRelationsUnderNot_stayEquivalent)
    {
        const Compiled compiled{ compile(FilterExpr::Not(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(genre7) })), FilterEntity::Track) };

        EXPECT_TRUE(compiled.joins.empty());
        EXPECT_EQ(compiled.where.getSql(), "NOT ((t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?))))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 7LL }));
    }

    TEST(FilterExprCompiler, relationsUnderAnd_areNotMerged)
    {
        const Compiled compiled{ compile(FilterExpr::Not(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::equal(genre7) })), FilterEntity::Track) };

        EXPECT_EQ(compiled.where.getSql(), "NOT ((" + std::string{ genreMember } + " AND " + std::string{ genreMember } + "))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 7LL }));
    }

    TEST(FilterExprCompiler, relationsInsideAnAndUnderOr_areNotMerged)
    {
        const Compiled compiled{ compile(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::And({ FilterExpr::equal(genre7), FilterExpr::equal(mood3) }) }), FilterEntity::Track) };

        EXPECT_EQ(compiled.where.getSql(), "(" + std::string{ genreMember } + " OR (" + std::string{ genreMember } + " AND " + std::string{ moodMember } + "))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 3LL }));
    }

    TEST(FilterExprCompiler, relationEqualToAConjunctUnderOr_isNotMerged)
    {
        const Compiled compiled{ compile(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(genre7), FilterExpr::equal(GenreId{ 9 }) }) }), FilterEntity::Track) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " JOIN track_genre f_j1 ON f_j1.track_id = t.id");
        EXPECT_EQ(compiled.where.getSql(), "(f_j1.genre_id = ? AND (f_j1.genre_id = ? OR t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?))))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 5LL, 7LL, 9LL }));
    }

    TEST(FilterExprCompiler, mergedReleaseRelations_followTheQueriedEntity)
    {
        const Compiled onReleases{ compile(FilterExpr::Or({ FilterExpr::equal(label2), FilterExpr::equal(LabelId{ 4 }) }), FilterEntity::Release) };
        EXPECT_EQ(onReleases.where.getSql(), "(r.id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id IN (?,?)))");

        const Compiled onTracks{ compile(FilterExpr::Or({ FilterExpr::equal(label2), FilterExpr::equal(LabelId{ 4 }) }), FilterEntity::Track) };
        EXPECT_EQ(onTracks.where.getSql(), "(t.release_id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id IN (?,?)))");
    }

    TEST(FilterExprCompiler, relationUnderNot_isAMembershipTest)
    {
        const Compiled compiled{ compile(FilterExpr::Not(FilterExpr::equal(genre5)), FilterEntity::Track) };

        EXPECT_TRUE(compiled.joins.empty());
        EXPECT_EQ(compiled.where.getSql(), "NOT (" + std::string{ genreMember } + ")");
    }

    TEST(FilterExprCompiler, releaseRelationOfTracksUnderNot_keepsTracksWithoutReleaseIn)
    {
        // A track without release has a NULL release id: NULL IN (...) would be unknown, that NOT keeps out
        const Compiled onTracks{ compile(FilterExpr::Not(FilterExpr::equal(label2)), FilterEntity::Track) };
        EXPECT_EQ(onTracks.where.getSql(), "NOT (COALESCE(t.release_id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id = ?), 0))");

        const Compiled onTracksMerged{ compile(FilterExpr::Not(FilterExpr::Or({ FilterExpr::equal(label2), FilterExpr::equal(LabelId{ 4 }) })), FilterEntity::Track) };
        EXPECT_EQ(onTracksMerged.where.getSql(), "NOT ((COALESCE(t.release_id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id IN (?,?)), 0)))");

        const Compiled onArtists{ compile(FilterExpr::Not(FilterExpr::Or({ FilterExpr::equal(label2), FilterExpr::equal(LabelId{ 4 }) })), FilterEntity::Artist) };
        EXPECT_EQ(onArtists.where.getSql(), "NOT ((COALESCE(t.release_id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id IN (?,?)), 0)))");

        const Compiled onReleases{ compile(FilterExpr::Not(FilterExpr::equal(label2)), FilterEntity::Release) };
        EXPECT_EQ(onReleases.where.getSql(), "NOT (r.id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id = ?))");
    }

    TEST(FilterExprCompiler, mergedRelations_keepTheBindsInTheOrderOfTheSql)
    {
        // The rock conjunct is a join: its siblings are merged around it
        const Compiled compiled{ compile(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::Or({ FilterExpr::equal(genre7), FilterExpr::equal(mood3), FilterExpr::equal(genre5), FilterExpr::equal(GenreId{ 9 }), FilterExpr::equal(MoodId{ 4 }) }) }), FilterEntity::Track) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " JOIN track_genre f_j1 ON f_j1.track_id = t.id");
        EXPECT_EQ(compiled.where.getSql(), "(f_j1.genre_id = ? AND (t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?)) OR t.id IN (SELECT f_e.track_id FROM track_mood f_e WHERE f_e.mood_id IN (?,?)) OR f_j1.genre_id = ?))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 9LL, 3LL, 4LL, 5LL }));
    }

    TEST(FilterExprCompiler, relationEqualToAConjunct_reusesItsJoinEvenUnderOr)
    {
        const Compiled compiled{ compile(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(mood3) }) }), FilterEntity::Track) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " JOIN track_genre f_j1 ON f_j1.track_id = t.id");
        EXPECT_EQ(compiled.where.getSql(), "(f_j1.genre_id = ? AND (f_j1.genre_id = ? OR " + std::string{ moodMember } + "))");
    }

    TEST(FilterExprCompiler, releaseRelation_followsTheQueriedEntity)
    {
        const Compiled onReleases{ compile(FilterExpr::equal(label2), FilterEntity::Release) };
        EXPECT_EQ(onReleases.joins.toSql().getSql(), " JOIN release_label f_j1 ON f_j1.release_id = r.id");
        EXPECT_EQ(onReleases.where.getSql(), "f_j1.label_id = ?");

        const Compiled onTracks{ compile(FilterExpr::equal(label2), FilterEntity::Track) };
        EXPECT_EQ(onTracks.joins.toSql().getSql(), " JOIN release_label f_j1 ON f_j1.release_id = t.release_id");
        EXPECT_EQ(onTracks.where.getSql(), "f_j1.label_id = ?");
    }

    TEST(FilterExprCompiler, trackConditionOnReleases_usesTheTracksJoinedByTheCaller)
    {
        const Compiled compiled{ compile(FilterExpr::equal(genre5), FilterEntity::Release) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " JOIN track t ON t.release_id = r.id JOIN track_genre f_j1 ON f_j1.track_id = t.id");
    }

    TEST(FilterExprCompiler, mediaLibraryUnderNot_keepsMissingValuesOut)
    {
        const Compiled compiled{ compile(FilterExpr::Not(FilterExpr::equal(MediaLibraryId{ 4 })), FilterEntity::Track) };

        EXPECT_TRUE(compiled.joins.empty());
        EXPECT_EQ(compiled.where.getSql(), "NOT (COALESCE(t.media_library_id = ?, 0))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 4LL }));
    }

    TEST(FilterExprCompiler, integerBetween_bindsBothBounds)
    {
        const Compiled compiled{ compile(FilterExpr::condition(IntegerCondition{ IntegerField::Year, FilterOperator::Between, FilterRange<std::int64_t>{ 1990, 1999 } }), FilterEntity::Track) };

        EXPECT_EQ(compiled.where.getSql(), "CAST(SUBSTR(t.date, 1, 4) AS INTEGER) BETWEEN ? AND ?");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 1990LL, 1999LL }));
    }

    TEST(FilterExprCompiler, stringCondition_bindsAnEscapedPattern)
    {
        const Compiled compiled{ compile(FilterExpr::condition(StringCondition{ StringField::TrackName, StringOperator::StartsWith, "100%" }), FilterEntity::Track) };

        EXPECT_EQ(compiled.where.getSql(), R"(t.name LIKE ? ESCAPE '\')");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ std::string{ R"(100\%%)" } }));
    }

    TEST(FilterExprCompiler, perUserCondition_joinsTheUserRowsAndBindsTheUserFirst)
    {
        const Compiled compiled{ compile(FilterExpr::condition(RatingCondition{ FilterOperator::GreaterOrEqual, std::int64_t{ 4 } }), FilterEntity::Track, UserId{ 9 }) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " LEFT JOIN rated_track f_rt ON f_rt.track_id = t.id AND f_rt.user_id = ?");
        EXPECT_EQ(compiled.joins.toSql().getBinds(), (std::vector<BindValue>{ 9LL }));
        EXPECT_EQ(compiled.where.getSql(), "f_rt.rating >= ?");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 4LL }));
    }

    TEST(FilterExprCompiler, listenCount_isAnIndexedSubqueryBindingTheUserBeforeTheOperand)
    {
        const Compiled compiled{ compile(FilterExpr::condition(ListenCountCondition{ FilterOperator::Greater, std::int64_t{ 0 } }), FilterEntity::Track, UserId{ 9 }) };

        EXPECT_TRUE(compiled.joins.empty());
        EXPECT_EQ(compiled.where.getSql(), "(SELECT COUNT(*) FROM listen f_l WHERE f_l.user_id = ? AND f_l.track_id = t.id) > ?");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ 9LL, 0LL }));
    }

    TEST(FilterExprCompiler, releaseNameOnTracks_joinsTheRelease)
    {
        const Compiled compiled{ compile(FilterExpr::condition(StringCondition{ StringField::ReleaseName, StringOperator::Equal, "Album" }), FilterEntity::Track) };

        EXPECT_EQ(compiled.joins.toSql().getSql(), " LEFT JOIN release f_rel ON f_rel.id = t.release_id");
        EXPECT_EQ(compiled.where.getSql(), R"(f_rel.name LIKE ? ESCAPE '\')");
    }

    TEST(FilterExprCompiler, releaseNameOnReleases_usesTheQueriedRelease)
    {
        const Compiled compiled{ compile(FilterExpr::condition(StringCondition{ StringField::ReleaseName, StringOperator::Equal, "Album" }), FilterEntity::Release) };

        EXPECT_TRUE(compiled.joins.empty());
        EXPECT_EQ(compiled.where.getSql(), R"(r.name LIKE ? ESCAPE '\')");
    }

    TEST(FilterExprCompiler, artistNameOnTracks_isAMembershipOfTheLinksOfTheTrackAndOfItsRelease)
    {
        const Compiled compiled{ compile(FilterExpr::condition(StringCondition{ StringField::ArtistName, StringOperator::Equal, "Artist" }), FilterEntity::Track) };

        EXPECT_TRUE(compiled.joins.empty());
        EXPECT_EQ(compiled.where.getSql(),
                  "(t.id IN (SELECT f_l.track_id FROM track_artist_link f_l WHERE f_l.artist_name LIKE ? ESCAPE '\\')"
                  " OR t.id IN (SELECT f_l.track_id FROM artist f_a JOIN track_artist_link f_l ON f_l.artist_id = f_a.id WHERE f_a.name LIKE ? ESCAPE '\\')"
                  " OR t.release_id IN (SELECT f_l.release_id FROM release_artist_link f_l WHERE f_l.artist_name LIKE ? ESCAPE '\\')"
                  " OR t.release_id IN (SELECT f_l.release_id FROM artist f_a JOIN release_artist_link f_l ON f_l.artist_id = f_a.id WHERE f_a.name LIKE ? ESCAPE '\\'))");
        EXPECT_EQ(compiled.where.getBinds(), (std::vector<BindValue>{ std::string{ "Artist" }, std::string{ "Artist" }, std::string{ "Artist" }, std::string{ "Artist" } }));
    }

    TEST(FilterExprCompiler, artistNameOnReleases_usesTheQueriedRelease)
    {
        const Compiled compiled{ compile(FilterExpr::condition(StringCondition{ StringField::ArtistName, StringOperator::Equal, "Artist" }), FilterEntity::Release) };

        EXPECT_EQ(compiled.where.getSql(),
                  "(t.id IN (SELECT f_l.track_id FROM track_artist_link f_l WHERE f_l.artist_name LIKE ? ESCAPE '\\')"
                  " OR t.id IN (SELECT f_l.track_id FROM artist f_a JOIN track_artist_link f_l ON f_l.artist_id = f_a.id WHERE f_a.name LIKE ? ESCAPE '\\')"
                  " OR r.id IN (SELECT f_l.release_id FROM release_artist_link f_l WHERE f_l.artist_name LIKE ? ESCAPE '\\')"
                  " OR r.id IN (SELECT f_l.release_id FROM artist f_a JOIN release_artist_link f_l ON f_l.artist_id = f_a.id WHERE f_a.name LIKE ? ESCAPE '\\'))");
    }

    TEST(FilterExprCompiler, artistNameOnArtists_isAMembershipOfThePairs)
    {
        const Compiled compiled{ compile(FilterExpr::condition(StringCondition{ StringField::ArtistName, StringOperator::Equal, "Artist" }), FilterEntity::Artist) };

        EXPECT_EQ(compiled.where.getSql(),
                  "((a.id, t.id) IN (SELECT f_l.artist_id, f_l.track_id FROM track_artist_link f_l WHERE f_l.artist_name LIKE ? ESCAPE '\\')"
                  " OR (a.id, t.id) IN (SELECT f_l.artist_id, f_l.track_id FROM artist f_a JOIN track_artist_link f_l ON f_l.artist_id = f_a.id WHERE f_a.name LIKE ? ESCAPE '\\')"
                  " OR (a.id, t.release_id) IN (SELECT f_l.artist_id, f_l.release_id FROM release_artist_link f_l WHERE f_l.artist_name LIKE ? ESCAPE '\\')"
                  " OR (a.id, t.release_id) IN (SELECT f_l.artist_id, f_l.release_id FROM artist f_a JOIN release_artist_link f_l ON f_l.artist_id = f_a.id WHERE f_a.name LIKE ? ESCAPE '\\'))");
    }

    TEST(FilterExprCompiler, artistNameUnderNot_keepsTracksWithoutReleaseIn)
    {
        const FilterExpr notArtistName{ FilterExpr::Not(FilterExpr::condition(StringCondition{ StringField::ArtistName, StringOperator::Equal, "Artist" })) };

        // A track without release has a NULL release id: NULL IN (...) would be unknown, that NOT keeps out
        const Compiled onTracks{ compile(notArtistName, FilterEntity::Track) };
        EXPECT_EQ(onTracks.where.getSql().substr(0, 16), "NOT (CASE WHEN (");
        EXPECT_EQ(onTracks.where.getSql().substr(onTracks.where.getSql().size() - 20), ") THEN 1 ELSE 0 END)");

        const Compiled onArtists{ compile(notArtistName, FilterEntity::Artist) };
        EXPECT_EQ(onArtists.where.getSql().substr(0, 16), "NOT (CASE WHEN (");

        const Compiled onReleases{ compile(notArtistName, FilterEntity::Release) };
        EXPECT_EQ(onReleases.where.getSql().substr(0, 16), "NOT (CASE WHEN (");
    }

    TEST(FilterExprCompiler, artistSortName_fallsBackOnTheNameOfTheLinkAndOfTheArtist)
    {
        const Compiled compiled{ compile(FilterExpr::condition(StringCondition{ StringField::ArtistSortName, StringOperator::StartsWith, "bea" }), FilterEntity::Track) };

        EXPECT_NE(compiled.where.getSql().find("COALESCE(NULLIF(f_l.artist_sort_name, ''), f_l.artist_name) LIKE ?"), std::string::npos);
        EXPECT_NE(compiled.where.getSql().find("COALESCE(NULLIF(f_a.sort_name, ''), f_a.name) LIKE ?"), std::string::npos);
        EXPECT_EQ(compiled.where.getBinds().front(), BindValue{ std::string{ "bea%" } });
    }

    TEST(FilterExprCompiler, requiresTracks_dependsOnTheQueriedEntity)
    {
        const FilterExpr trackCondition{ FilterExpr::equal(genre5) };
        const FilterExpr releaseCondition{ FilterExpr::equal(label2) };
        const FilterExpr releaseNameCondition{ FilterExpr::condition(StringCondition{ StringField::ReleaseName, StringOperator::Equal, "Album" }) };
        const FilterExpr artistNameCondition{ FilterExpr::condition(StringCondition{ StringField::ArtistName, StringOperator::Equal, "Artist" }) };

        EXPECT_TRUE(requiresTracks(releaseCondition, FilterEntity::Track));
        EXPECT_TRUE(requiresTracks(trackCondition, FilterEntity::Track));

        EXPECT_FALSE(requiresTracks(releaseCondition, FilterEntity::Release));
        EXPECT_FALSE(requiresTracks(releaseNameCondition, FilterEntity::Release));
        EXPECT_TRUE(requiresTracks(trackCondition, FilterEntity::Release));
        EXPECT_TRUE(requiresTracks(artistNameCondition, FilterEntity::Release));

        EXPECT_TRUE(requiresTracks(releaseCondition, FilterEntity::Artist));
        EXPECT_TRUE(requiresTracks(trackCondition, FilterEntity::Artist));
    }

    TEST(FilterExprCompiler, requiresTracks_lookupsInsideOperators)
    {
        const FilterExpr onlyReleaseConditions{ FilterExpr::Not(FilterExpr::And({ FilterExpr::equal(label2), FilterExpr::condition(IsCompilationCondition{ true }) })) };
        EXPECT_FALSE(requiresTracks(onlyReleaseConditions, FilterEntity::Release));

        const FilterExpr withTrackCondition{ FilterExpr::Or({ FilterExpr::equal(label2), FilterExpr::equal(genre5) }) };
        EXPECT_TRUE(requiresTracks(withTrackCondition, FilterEntity::Release));
    }
} // namespace lms::db::tests
