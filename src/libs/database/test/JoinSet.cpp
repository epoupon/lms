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

#include <vector>

#include <gtest/gtest.h>

#include "JoinSet.hpp"

namespace lms::db::tests
{
    TEST(JoinSet, default_isEmpty)
    {
        const JoinSet joins;
        EXPECT_TRUE(joins.empty());
        EXPECT_TRUE(joins.toSql().isEmpty());
    }

    TEST(JoinSet, toSql_insertionOrderAndJoinTypes)
    {
        JoinSet joins;
        joins.add("track t ON t.release_id = r.id");
        joins.add("medium m ON m.release_id = r.id", JoinSet::Type::Left);
        joins.add("directory d ON t.directory_id = d.id");

        EXPECT_FALSE(joins.empty());
        EXPECT_EQ(joins.toSql().getSql(), " JOIN track t ON t.release_id = r.id LEFT JOIN medium m ON m.release_id = r.id JOIN directory d ON t.directory_id = d.id");
        EXPECT_TRUE(joins.toSql().getBinds().empty());
    }

    TEST(JoinSet, toSql_bindsInTextualOrder)
    {
        JoinSet joins;
        joins.add("rated_track f_rt ON f_rt.track_id = t.id AND f_rt.user_id = ?", JoinSet::Type::Left, { 1LL });
        joins.add("track_feedback f_tf ON f_tf.track_id = t.id AND f_tf.user_id = ?", JoinSet::Type::Left, { 2LL });

        EXPECT_EQ(joins.toSql().getBinds(), (std::vector<BindValue>{ 1LL, 2LL }));
    }

    TEST(JoinSet, add_sameJoinTwice_isEmittedOnce)
    {
        JoinSet joins;
        joins.add("track t ON t.release_id = r.id");
        joins.add("medium m ON m.release_id = r.id", JoinSet::Type::Left);
        joins.add("track t ON t.release_id = r.id");

        EXPECT_EQ(joins.toSql().getSql(), " JOIN track t ON t.release_id = r.id LEFT JOIN medium m ON m.release_id = r.id");
    }

    TEST(JoinSet, add_sameJoinWithBindsTwice_isEmittedOnce)
    {
        JoinSet joins;
        joins.add("rated_track f_rt ON f_rt.track_id = t.id AND f_rt.user_id = ?", JoinSet::Type::Left, { 1LL });
        joins.add("rated_track f_rt ON f_rt.track_id = t.id AND f_rt.user_id = ?", JoinSet::Type::Left, { 1LL });

        EXPECT_EQ(joins.toSql().getBinds().size(), 1);
    }

    TEST(JoinSet, subqueryJoin_aliasIsTheOneOfTheSubquery)
    {
        JoinSet joins;
        joins.add("(SELECT a.id AS artist_id FROM artist a JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id WHERE t_a_l.type = ?) f_u ON f_u.artist_id = a.id", JoinSet::Type::Inner, { 1 });
        joins.add("track_artist_link t_a_l ON t_a_l.artist_id = a.id");

        EXPECT_EQ(joins.toSql().getSql(), " JOIN (SELECT a.id AS artist_id FROM artist a JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id WHERE t_a_l.type = ?) f_u ON f_u.artist_id = a.id JOIN track_artist_link t_a_l ON t_a_l.artist_id = a.id");
        EXPECT_EQ(joins.toSql().getBinds(), (std::vector<BindValue>{ 1 }));
    }

} // namespace lms::db::tests
