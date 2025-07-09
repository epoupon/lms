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

#include "database/objects/StarredRelease.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedStarredRelease = ScopedEntity<db::StarredRelease>;

    TEST_F(DatabaseFixture, StarredRelease)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto starredRelease{ StarredRelease::find(session, release->getId(), user->getId(), FeedbackBackend::Internal) };
            EXPECT_FALSE(starredRelease);
            EXPECT_EQ(StarredRelease::getCount(session), 0);

            auto releases{ Release::find(session, Release::FindParameters{}) };
            EXPECT_EQ(releases.results.size(), 1);
        }

        ScopedStarredRelease starredRelease{ session, release.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };
        {
            auto transaction{ session.createReadTransaction() };

            auto gotRelease{ StarredRelease::find(session, release->getId(), user->getId(), FeedbackBackend::Internal) };
            EXPECT_EQ(gotRelease->getId(), starredRelease->getId());
            EXPECT_EQ(StarredRelease::getCount(session), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::find(session, Release::FindParameters{}) };
            EXPECT_EQ(releases.results.size(), 1);

            releases = Release::find(session, Release::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(releases.results.size(), 1);

            releases = Release::find(session, Release::FindParameters{}.setStarringUser(user2.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(releases.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            user.get().modify()->setFeedbackBackend(FeedbackBackend::ListenBrainz);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto gotRelease{ StarredRelease::find(session, release->getId(), user->getId()) };
            EXPECT_EQ(gotRelease, StarredRelease::pointer{});
        }
    }

    TEST_F(DatabaseFixture, Starredrelease_PendingDestroy)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedUser user{ session, "MyUser" };
        ScopedStarredRelease starredRelease{ session, release.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };

        {
            auto transaction{ session.createWriteTransaction() };

            auto releases{ Release::find(session, Release::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal)) };
            EXPECT_EQ(releases.results.size(), 1);

            starredRelease.get().modify()->setSyncState(SyncState::PendingRemove);
            releases = Release::find(session, Release::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(releases.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, StarredRelease_dateTime)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        ScopedUser user{ session, "MyUser" };

        ScopedStarredRelease starredRelease1{ session, release1.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };
        ScopedStarredRelease starredRelease2{ session, release2.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };

        const Wt::WDateTime dateTime{ Wt::WDate{ 1950, 1, 2 }, Wt::WTime{ 12, 30, 1 } };

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal)) };
            EXPECT_EQ(releases.results.size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            starredRelease1.get().modify()->setDateTime(dateTime);
            starredRelease2.get().modify()->setDateTime(dateTime.addSecs(-1));

            auto releases{ Release::findIds(session, Release::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal).setSortMethod(ReleaseSortMethod::StarredDateDesc)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0], starredRelease1->getRelease()->getId());
            EXPECT_EQ(releases.results[1], starredRelease2->getRelease()->getId());
        }
        {
            auto transaction{ session.createWriteTransaction() };

            starredRelease1.get().modify()->setDateTime(dateTime);
            starredRelease2.get().modify()->setDateTime(dateTime.addSecs(1));

            auto releases{ Release::findIds(session, Release::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal).setSortMethod(ReleaseSortMethod::StarredDateDesc)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0], starredRelease2->getRelease()->getId());
            EXPECT_EQ(releases.results[1], starredRelease1->getRelease()->getId());
        }
    }
} // namespace lms::db::tests