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

#include "SqlFragment.hpp"
#include "objects/FilterExprCompiler.hpp"

namespace lms::db::tests
{
    namespace
    {
        const GenreId genre5{ 5 };
        const GenreId genre7{ 7 };
        const MoodId mood3{ 3 };
        const LabelId label2{ 2 };

        const std::string genreMember{ "t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id = ?)" };
        const std::string moodMember{ "t.id IN (SELECT f_e.track_id FROM track_mood f_e WHERE f_e.mood_id = ?)" };
        const std::string genreMemberUnderNot{ "CASE WHEN " + genreMember + " THEN 1 ELSE 0 END" };
    } // namespace

    TEST(FilterExprCompiler, matchingTracks_selectTheGivenColumnOfTheTracks)
    {
        const SqlFragment compiled{ createMatchingTracksCondition("r.id", FilterExpr::equal(genre5), UserId{}, "t.release_id") };

        EXPECT_EQ(compiled.getSql(), "r.id IN (SELECT t.release_id FROM track t WHERE " + genreMember + ")");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL }));
    }

    TEST(FilterExprCompiler, relation_isAMembershipTest)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::equal(genre5), UserId{}) };

        EXPECT_EQ(compiled.getSql(), genreMember);
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL }));
    }

    TEST(FilterExprCompiler, relationsUnderAnd_areMembershipTestsOfTheirOwn)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::equal(genre7), FilterExpr::equal(mood3) }), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "(" + genreMember + " AND " + genreMember + " AND " + moodMember + ")");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 3LL }));
    }

    TEST(FilterExprCompiler, relationsUnderOr_areMembershipTests)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(mood3) }), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "(" + genreMember + " OR " + moodMember + ")");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 3LL }));
    }

    TEST(FilterExprCompiler, sameRelationsUnderOr_areMergedIntoOneMembershipTest)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(genre7), FilterExpr::equal(GenreId{ 9 }) }), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "(t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?,?)))");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 9LL }));
    }

    TEST(FilterExprCompiler, mergedRelations_takeThePlaceOfTheFirstOne)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(mood3), FilterExpr::equal(genre7) }), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "(t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?)) OR " + moodMember + ")");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 3LL }));
    }

    TEST(FilterExprCompiler, relationsOfDifferentTypesUnderOr_areMergedPerType)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(mood3), FilterExpr::equal(GenreId{ 9 }), FilterExpr::equal(MoodId{ 4 }) }), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "(t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?)) OR t.id IN (SELECT f_e.track_id FROM track_mood f_e WHERE f_e.mood_id IN (?,?)))");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 9LL, 3LL, 4LL }));
    }

    TEST(FilterExprCompiler, mergedRelationsUnderNot_stayEquivalent)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Not(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::equal(genre7) })), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "NOT ((CASE WHEN t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?)) THEN 1 ELSE 0 END))");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 7LL }));
    }

    TEST(FilterExprCompiler, relationsUnderAnd_areNotMerged)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Not(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::equal(genre7) })), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "NOT ((" + genreMemberUnderNot + " AND " + genreMemberUnderNot + "))");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 7LL }));
    }

    TEST(FilterExprCompiler, relationsInsideAnAndUnderOr_areNotMerged)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Or({ FilterExpr::equal(genre5), FilterExpr::And({ FilterExpr::equal(genre7), FilterExpr::equal(mood3) }) }), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "(" + genreMember + " OR (" + genreMember + " AND " + moodMember + "))");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 3LL }));
    }

    TEST(FilterExprCompiler, mergedRelations_keepTheBindsInTheOrderOfTheSql)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::And({ FilterExpr::equal(genre5), FilterExpr::Or({ FilterExpr::equal(genre7), FilterExpr::equal(mood3), FilterExpr::equal(genre5), FilterExpr::equal(GenreId{ 9 }), FilterExpr::equal(MoodId{ 4 }) }) }), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "(" + genreMember + " AND (t.id IN (SELECT f_e.track_id FROM track_genre f_e WHERE f_e.genre_id IN (?,?,?)) OR t.id IN (SELECT f_e.track_id FROM track_mood f_e WHERE f_e.mood_id IN (?,?))))");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 5LL, 7LL, 5LL, 9LL, 3LL, 4LL }));
    }

    TEST(FilterExprCompiler, releaseRelation_isAMembershipOfTheReleaseOfTheTrack)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::equal(label2), UserId{}) };
        EXPECT_EQ(compiled.getSql(), "t.release_id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id = ?)");

        const SqlFragment merged{ compileFilterExpr(FilterExpr::Or({ FilterExpr::equal(label2), FilterExpr::equal(LabelId{ 4 }) }), UserId{}) };
        EXPECT_EQ(merged.getSql(), "(t.release_id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id IN (?,?)))");
    }

    TEST(FilterExprCompiler, relationUnderNot_isAMembershipTest)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Not(FilterExpr::equal(genre5)), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "NOT (" + genreMemberUnderNot + ")");
    }

    TEST(FilterExprCompiler, releaseRelationUnderNot_keepsTracksWithoutReleaseIn)
    {
        // A track without release has a NULL release id: NULL IN (...) would be unknown, that NOT keeps out
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Not(FilterExpr::equal(label2)), UserId{}) };
        EXPECT_EQ(compiled.getSql(), "NOT (CASE WHEN t.release_id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id = ?) THEN 1 ELSE 0 END)");

        const SqlFragment merged{ compileFilterExpr(FilterExpr::Not(FilterExpr::Or({ FilterExpr::equal(label2), FilterExpr::equal(LabelId{ 4 }) })), UserId{}) };
        EXPECT_EQ(merged.getSql(), "NOT ((CASE WHEN t.release_id IN (SELECT f_e.release_id FROM release_label f_e WHERE f_e.label_id IN (?,?)) THEN 1 ELSE 0 END))");
    }

    TEST(FilterExprCompiler, compilation_isAMembershipOfTheReleaseOfTheTrack)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::condition(IsCompilationCondition{ true }), UserId{}) };
        EXPECT_EQ(compiled.getSql(), "t.release_id IN (SELECT f_r.id FROM release f_r WHERE f_r.is_compilation = ?)");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ true }));

        // A track without release has a NULL release id: NULL IN (...) would be unknown, that NOT keeps out
        const SqlFragment underNot{ compileFilterExpr(FilterExpr::Not(FilterExpr::condition(IsCompilationCondition{ true })), UserId{}) };
        EXPECT_EQ(underNot.getSql(), "NOT (CASE WHEN t.release_id IN (SELECT f_r.id FROM release f_r WHERE f_r.is_compilation = ?) THEN 1 ELSE 0 END)");
    }

    TEST(FilterExprCompiler, mediaLibraryUnderNot_keepsMissingValuesOut)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::Not(FilterExpr::equal(MediaLibraryId{ 4 })), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "NOT (CASE WHEN t.media_library_id = ? THEN 1 ELSE 0 END)");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 4LL }));
    }

    TEST(FilterExprCompiler, integerBetween_bindsBothBounds)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::condition(IntegerCondition{ IntegerField::Year, FilterOperator::Between, FilterRange<std::int64_t>{ 1990, 1999 } }), UserId{}) };

        EXPECT_EQ(compiled.getSql(), "CAST(SUBSTR(t.date, 1, 4) AS INTEGER) BETWEEN ? AND ?");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 1990LL, 1999LL }));
    }

    TEST(FilterExprCompiler, rating_isAMembershipBindingTheUserFirst)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::condition(RatingCondition{ FilterOperator::GreaterOrEqual, std::int64_t{ 4 } }), UserId{ 9 }) };

        EXPECT_EQ(compiled.getSql(), "t.id IN (SELECT f_rt.track_id FROM rated_track f_rt WHERE f_rt.user_id = ? AND f_rt.rating >= ?)");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 9LL, 4LL }));
    }

    TEST(FilterExprCompiler, feedback_isAMembershipBindingTheUserFirst)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::condition(FeedbackCondition{ FeedbackValue::Loved }), UserId{ 9 }) };

        EXPECT_EQ(compiled.getSql(), "t.id IN (SELECT f_tf.track_id FROM track_feedback f_tf WHERE f_tf.user_id = ? AND f_tf.value = ?)");
        ASSERT_EQ(compiled.getBinds().size(), 2);
        EXPECT_EQ(compiled.getBinds().front(), BindValue{ 9LL });
    }

    TEST(FilterExprCompiler, listenCount_isAnIndexedSubqueryBindingTheUserBeforeTheOperand)
    {
        const SqlFragment compiled{ compileFilterExpr(FilterExpr::condition(ListenCountCondition{ FilterOperator::Greater, std::int64_t{ 0 } }), UserId{ 9 }) };

        EXPECT_EQ(compiled.getSql(), "(SELECT COUNT(*) FROM listen f_l WHERE f_l.user_id = ? AND f_l.track_id = t.id) > ?");
        EXPECT_EQ(compiled.getBinds(), (std::vector<BindValue>{ 9LL, 0LL }));
    }
} // namespace lms::db::tests
