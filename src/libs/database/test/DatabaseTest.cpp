/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <list>

#include "Common.hpp"

namespace lms::db::tests
{
    TEST_F(DatabaseFixture, MultiTracksSingleArtistSingleRelease)
    {
        constexpr std::size_t nbTracks{ 10 };
        std::list<ScopedTrack> tracks;
        ScopedArtist artist{ session, "MyArtst" };
        ScopedRelease release{ session, "MyRelease" };

        for (std::size_t i{}; i < nbTracks; ++i)
        {
            tracks.emplace_back(session);

            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLinkType::Artist);
            tracks.back().get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_TRUE(Release::findOrphanIds(session).results.empty());
            EXPECT_TRUE(Artist::findOrphanIds(session).results.empty());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters {}.setArtist(artist.getId())) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());

            const auto releaseTracks{ Track::find(session, Track::FindParameters {}.setRelease(release.getId())) };
            EXPECT_EQ(releaseTracks.results.size(), nbTracks);
        }
    }

    TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtist)
    {
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createWriteTransaction() };

            auto trackArtistLink{ TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist) };
            track.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters {}.setArtist(artist.getId())) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());

            auto artists{ release->getArtists() };
            ASSERT_EQ(artists.size(), 1);
            ASSERT_EQ(artists.front()->getId(), artist.getId());
        }
    }

    TEST_F(DatabaseFixture, SingleUser)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_TRUE(User::find(session, User::FindParameters{}).results.empty());
            EXPECT_EQ(User::getCount(session), 0);
        }

        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(User::find(session, User::FindParameters{}).results.size(), 1);
            EXPECT_EQ(User::getCount(session), 1);
        }
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}