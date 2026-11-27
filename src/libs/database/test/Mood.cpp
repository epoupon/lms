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
    using ScopedMood = ScopedEntity<db::Mood>;

    TEST_F(DatabaseFixture, Mood_create)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Mood::getCount(session), 0);
        }

        ScopedMood mood{ session, "Happy" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Mood::getCount(session), 1);

            const std::optional<Mood> found{ Mood::find(session, mood.getId()) };
            ASSERT_TRUE(found);
            EXPECT_EQ(found->getName(), "Happy");
        }
    }

    TEST_F(DatabaseFixture, Mood_findByName)
    {
        ScopedMood mood{ session, "Chill" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_TRUE(Mood::find(session, "Chill"));
            EXPECT_FALSE(Mood::find(session, "chill"));
            EXPECT_FALSE(Mood::find(session, ""));
            EXPECT_FALSE(Mood::find(session, "Chil"));
        }
    }

    TEST_F(DatabaseFixture, Mood_orphan)
    {
        ScopedMood mood{ session, "Sad" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto orphans{ Mood::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), mood.getId());
        }
    }

    TEST_F(DatabaseFixture, Mood_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedMood mood1{ session, "Dark" };
        ScopedMood mood2{ session, "Upbeat" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Mood::findOrphanIds(session).size(), 2);
            EXPECT_EQ(Mood::findIds(session, Mood::FindParameters{}.setTrack(track.getId())).size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Track::setMoods(session, track.getId(), std::array{ mood1.getId() });
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto moods{ Mood::findIds(session, Mood::FindParameters{}.setTrack(track.getId())) };
            ASSERT_EQ(moods.size(), 1);
            EXPECT_EQ(moods.front(), mood1.getId());

            const auto orphans{ Mood::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), mood2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilter(FilterExpr::equal(mood1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            const auto tracks2{ Track::findIds(session, Track::FindParameters{}.setFilter(FilterExpr::equal(mood2.getId()))) };
            EXPECT_EQ(tracks2.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Mood_multipleMoodsOnTrack)
    {
        ScopedTrack track{ session };
        ScopedMood mood1{ session, "Melancholic" };
        ScopedMood mood2{ session, "Romantic" };

        {
            auto transaction{ session.createWriteTransaction() };
            Track::setMoods(session, track.getId(), std::array{ mood1.getId(), mood2.getId() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Mood::findOrphanIds(session).size(), 0);

            const auto trackMoods{ Mood::findIds(session, Mood::FindParameters{}.setTrack(track.getId())) };
            EXPECT_EQ(trackMoods.size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilter(FilterExpr::equal(mood1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            const auto tracks2{ Track::findIds(session, Track::FindParameters{}.setFilter(FilterExpr::equal(mood2.getId()))) };
            ASSERT_EQ(tracks2.size(), 1);
            EXPECT_EQ(tracks2.front(), track.getId());
        }
    }

    TEST_F(DatabaseFixture, Mood_sortByName)
    {
        ScopedMood m1{ session, "Zzz" };
        ScopedMood m2{ session, "Aaa" };
        ScopedMood m3{ session, "Mmm" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto moods{ Mood::findIds(session, Mood::FindParameters{}.setSortMethod(MoodSortMethod::Name)) };
            ASSERT_EQ(moods.size(), 3);
            EXPECT_EQ(moods[0], m2.getId());
            EXPECT_EQ(moods[1], m3.getId());
            EXPECT_EQ(moods[2], m1.getId());
        }
    }

    TEST_F(DatabaseFixture, Mood_sortByTrackCount)
    {
        ScopedMood m1{ session, "Energetic" };
        ScopedMood m2{ session, "Chill" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            Track::setMoods(session, track1.getId(), std::array{ m1.getId() });
            Track::setMoods(session, track2.getId(), std::array{ m1.getId() });
            Track::setMoods(session, track3.getId(), std::array{ m2.getId() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto moods{ Mood::findIds(session, Mood::FindParameters{}.setSortMethod(MoodSortMethod::TrackCountDesc)) };
            ASSERT_EQ(moods.size(), 2);
            EXPECT_EQ(moods[0], m1.getId());
            EXPECT_EQ(moods[1], m2.getId());
        }
    }

    TEST_F(DatabaseFixture, Mood_sortByTrackCount_artist)
    {
        ScopedMood m1{ session, "Energetic" };
        ScopedMood m2{ session, "Chill" };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            // linked three times to the same track: it still is a single track, counted per link m1 would come first
            TrackArtistLink::create(session, track1.getId(), artist.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track1.getId(), artist.getId(), TrackArtistLinkType::Composer);
            TrackArtistLink::create(session, track1.getId(), artist.getId(), TrackArtistLinkType::Lyricist);
            TrackArtistLink::create(session, track2.getId(), artist.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track3.getId(), artist.getId(), TrackArtistLinkType::Artist);
            Track::setMoods(session, track1.getId(), std::array{ m1.getId() });
            Track::setMoods(session, track2.getId(), std::array{ m2.getId() });
            Track::setMoods(session, track3.getId(), std::array{ m2.getId() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto moods{ Mood::findIds(session, Mood::FindParameters{}.setArtist(artist.getId()).setSortMethod(MoodSortMethod::TrackCountDesc)) };
            ASSERT_EQ(moods.size(), 2);
            EXPECT_EQ(moods[0], m2.getId());
            EXPECT_EQ(moods[1], m1.getId());
        }
    }
} // namespace lms::db::tests
