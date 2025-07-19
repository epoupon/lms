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

#include "database/objects/StarredTrack.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedStarredTrack = ScopedEntity<db::StarredTrack>;

    TEST_F(DatabaseFixture, StarredTrack)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto starredTrack{ StarredTrack::find(session, track->getId(), user->getId(), FeedbackBackend::Internal) };
            EXPECT_FALSE(starredTrack);
            EXPECT_EQ(StarredTrack::getCount(session), 0);

            auto tracks{ Track::findIds(session, Track::FindParameters{}) };
            EXPECT_EQ(tracks.results.size(), 1);
        }

        ScopedStarredTrack starredTrack{ session, track.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };
        {
            auto transaction{ session.createReadTransaction() };

            auto gotTrack{ StarredTrack::find(session, track->getId(), user->getId(), FeedbackBackend::Internal) };
            EXPECT_EQ(gotTrack->getId(), starredTrack->getId());
            EXPECT_EQ(StarredTrack::getCount(session), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}) };
            EXPECT_EQ(tracks.results.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(tracks.results.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setStarringUser(user2.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(tracks.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            user.get().modify()->setFeedbackBackend(FeedbackBackend::ListenBrainz);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto gotRelease{ StarredTrack::find(session, track->getId(), user->getId()) };
            EXPECT_EQ(gotRelease, StarredTrack::pointer{});
        }
    }

    TEST_F(DatabaseFixture, Starredtrack_PendingDestroy)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedStarredTrack starredTrack{ session, track.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };

        {
            auto transaction{ session.createWriteTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal)) };
            EXPECT_EQ(tracks.results.size(), 1);

            starredTrack.get().modify()->setSyncState(SyncState::PendingRemove);
            tracks = Track::findIds(session, Track::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(tracks.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, StarredTrack_dateTime)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };

        ScopedStarredTrack starredTrack1{ session, track1.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };
        ScopedStarredTrack starredTrack2{ session, track2.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };

        const Wt::WDateTime dateTime{ Wt::WDate{ 1950, 1, 2 }, Wt::WTime{ 12, 30, 1 } };

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal)) };
            EXPECT_EQ(tracks.results.size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            starredTrack1.get().modify()->setDateTime(dateTime);
            starredTrack2.get().modify()->setDateTime(dateTime.addSecs(-1));

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal).setSortMethod(TrackSortMethod::StarredDateDesc)) };
            ASSERT_EQ(tracks.results.size(), 2);
            EXPECT_EQ(tracks.results[0], starredTrack1->getTrack()->getId());
            EXPECT_EQ(tracks.results[1], starredTrack2->getTrack()->getId());
        }
        {
            auto transaction{ session.createWriteTransaction() };

            starredTrack1.get().modify()->setDateTime(dateTime);
            starredTrack2.get().modify()->setDateTime(dateTime.addSecs(1));

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal).setSortMethod(TrackSortMethod::StarredDateDesc)) };
            ASSERT_EQ(tracks.results.size(), 2);
            EXPECT_EQ(tracks.results[0], starredTrack2->getTrack()->getId());
            EXPECT_EQ(tracks.results[1], starredTrack1->getTrack()->getId());
        }
    }
} // namespace lms::db::tests