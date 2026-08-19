/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedGrouping = ScopedEntity<db::Grouping>;

    TEST_F(DatabaseFixture, Grouping_create)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Grouping::getCount(session), 0);
        }

        ScopedGrouping grouping{ session, "Soundtrack" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Grouping::getCount(session), 1);

            const Grouping::pointer found{ Grouping::find(session, grouping.getId()) };
            ASSERT_TRUE(found);
            EXPECT_EQ(found->getName(), "Soundtrack");
        }
    }

    TEST_F(DatabaseFixture, Grouping_findByName)
    {
        ScopedGrouping grouping{ session, "Live" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_TRUE(Grouping::find(session, "Live"));
            EXPECT_FALSE(Grouping::find(session, "live"));
            EXPECT_FALSE(Grouping::find(session, ""));
            EXPECT_FALSE(Grouping::find(session, "Liv"));
        }
    }

    TEST_F(DatabaseFixture, Grouping_orphan)
    {
        ScopedGrouping grouping{ session, "Podcast" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto orphans{ Grouping::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), grouping.getId());
        }
    }

    TEST_F(DatabaseFixture, Grouping_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedGrouping grouping1{ session, "Audiobook" };
        ScopedGrouping grouping2{ session, "Remix" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Grouping::findOrphanIds(session).size(), 2);
            EXPECT_EQ(track->getGroupings().size(), 0);
            EXPECT_EQ(track->getGroupingIds().size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setGroupings(std::array{ grouping1.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto groupings{ Grouping::findIds(session, Grouping::FindParameters{}.setTrack(track.getId())) };
            ASSERT_EQ(groupings.size(), 1);
            EXPECT_EQ(groupings.front(), grouping1.getId());

            const auto orphans{ Grouping::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), grouping2.getId());

            const auto trackGroupings{ track->getGroupings() };
            ASSERT_EQ(trackGroupings.size(), 1);
            EXPECT_EQ(trackGroupings.front()->getId(), grouping1.getId());

            const auto trackGroupingIds{ track->getGroupingIds() };
            ASSERT_EQ(trackGroupingIds.size(), 1);
            EXPECT_EQ(trackGroupingIds.front(), grouping1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setGrouping(grouping1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            const auto tracks2{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setGrouping(grouping2.getId()))) };
            EXPECT_EQ(tracks2.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Grouping_multipleGroupingsOnTrack)
    {
        ScopedTrack track{ session };
        ScopedGrouping grouping1{ session, "Soundtrack" };
        ScopedGrouping grouping2{ session, "Live" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setGroupings(std::array{ grouping1.get(), grouping2.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Grouping::findOrphanIds(session).size(), 0);

            const auto trackGroupings{ track->getGroupings() };
            EXPECT_EQ(trackGroupings.size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setGrouping(grouping1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            const auto tracks2{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setGrouping(grouping2.getId()))) };
            ASSERT_EQ(tracks2.size(), 1);
            EXPECT_EQ(tracks2.front(), track.getId());
        }
    }

    TEST_F(DatabaseFixture, Grouping_sortByName)
    {
        ScopedGrouping g1{ session, "Zzz" };
        ScopedGrouping g2{ session, "Aaa" };
        ScopedGrouping g3{ session, "Mmm" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto groupings{ Grouping::findIds(session, Grouping::FindParameters{}.setSortMethod(GroupingSortMethod::Name)) };
            ASSERT_EQ(groupings.size(), 3);
            EXPECT_EQ(groupings[0], g2.getId());
            EXPECT_EQ(groupings[1], g3.getId());
            EXPECT_EQ(groupings[2], g1.getId());
        }
    }

    TEST_F(DatabaseFixture, Grouping_sortByTrackCount)
    {
        ScopedGrouping g1{ session, "Compilation" };
        ScopedGrouping g2{ session, "Live" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setGroupings(std::array{ g1.get() });
            track2.get().modify()->setGroupings(std::array{ g1.get() });
            track3.get().modify()->setGroupings(std::array{ g2.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto groupings{ Grouping::findIds(session, Grouping::FindParameters{}.setSortMethod(GroupingSortMethod::TrackCountDesc)) };
            ASSERT_EQ(groupings.size(), 2);
            EXPECT_EQ(groupings[0], g1.getId());
            EXPECT_EQ(groupings[1], g2.getId());
        }
    }
} // namespace lms::db::tests
