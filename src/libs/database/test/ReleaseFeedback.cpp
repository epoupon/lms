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

#include "database/objects/ReleaseFeedback.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedReleaseFeedback = ScopedEntity<db::ReleaseFeedback>;

    TEST_F(DatabaseFixture, ReleaseFeedback)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto releaseFeedback{ ReleaseFeedback::find(session, release->getId(), user->getId()) };
            EXPECT_FALSE(releaseFeedback);
            EXPECT_EQ(ReleaseFeedback::getCount(session), 0);

            auto releases{ Release::find(session, Release::FindParameters{}) };
            EXPECT_EQ(releases.size(), 1);
        }

        ScopedReleaseFeedback releaseFeedback{ session, release.lockAndGet(), user.lockAndGet() };
        {
            auto transaction{ session.createWriteTransaction() };

            auto gotRelease{ ReleaseFeedback::find(session, release->getId(), user->getId()) };
            EXPECT_EQ(gotRelease->getId(), releaseFeedback->getId());
            EXPECT_EQ(ReleaseFeedback::getCount(session), 1);
            EXPECT_EQ(gotRelease->getValue(), FeedbackValue::None);

            releaseFeedback.get().modify()->setValue(FeedbackValue::Loved);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::find(session, Release::FindParameters{}) };
            EXPECT_EQ(releases.size(), 1);

            releases = Release::find(session, Release::FindParameters{}.setFeedbackUser(user.getId()));
            EXPECT_EQ(releases.size(), 1);

            releases = Release::find(session, Release::FindParameters{}.setFeedbackUser(user2.getId()));
            EXPECT_EQ(releases.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, ReleaseFeedback_ClearedValueExcluded)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedUser user{ session, "MyUser" };
        ScopedReleaseFeedback releaseFeedback{ session, release.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            releaseFeedback.get().modify()->setValue(FeedbackValue::Loved);
            auto releases{ Release::find(session, Release::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved)) };
            EXPECT_EQ(releases.size(), 1);

            releaseFeedback.get().modify()->setValue(FeedbackValue::None);
            releases = Release::find(session, Release::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved));
            EXPECT_EQ(releases.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, ReleaseFeedback_HatedExcluded)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedUser user{ session, "MyUser" };
        ScopedReleaseFeedback releaseFeedback{ session, release.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            releaseFeedback.get().modify()->setValue(FeedbackValue::Hated);
            auto releases{ Release::find(session, Release::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved)) };
            EXPECT_EQ(releases.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, ReleaseFeedback_AnyValueIncludedWhenValueNotSet)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedUser user{ session, "MyUser" };
        ScopedReleaseFeedback releaseFeedback{ session, release.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            releaseFeedback.get().modify()->setValue(FeedbackValue::Hated);
            auto releases{ Release::find(session, Release::FindParameters{}.setFeedbackUser(user.getId())) };
            EXPECT_EQ(releases.size(), 1);
        }
    }

    TEST_F(DatabaseFixture, ReleaseFeedback_ValueFilterAcrossUsers)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        ScopedUser user1{ session, "MyUser1" };
        ScopedUser user2{ session, "MyUser2" };
        ScopedReleaseFeedback releaseFeedback1{ session, release1.lockAndGet(), user1.lockAndGet() };
        ScopedReleaseFeedback releaseFeedback2{ session, release2.lockAndGet(), user2.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            releaseFeedback1.get().modify()->setValue(FeedbackValue::Loved);
            releaseFeedback2.get().modify()->setValue(FeedbackValue::Hated);

            auto releases{ Release::findIds(session, Release::FindParameters{}.setFeedbackValue(FeedbackValue::Loved)) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release1->getId());
        }
    }

    TEST_F(DatabaseFixture, ReleaseFeedback_dateTime)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        ScopedUser user{ session, "MyUser" };

        ScopedReleaseFeedback releaseFeedback1{ session, release1.lockAndGet(), user.lockAndGet() };
        ScopedReleaseFeedback releaseFeedback2{ session, release2.lockAndGet(), user.lockAndGet() };

        const Wt::WDateTime dateTime{ Wt::WDate{ 1950, 1, 2 }, Wt::WTime{ 12, 30, 1 } };

        {
            auto transaction{ session.createWriteTransaction() };

            releaseFeedback1.get().modify()->setValue(FeedbackValue::Loved);
            releaseFeedback2.get().modify()->setValue(FeedbackValue::Loved);

            auto releases{ Release::findIds(session, Release::FindParameters{}.setFeedbackUser(user.getId())) };
            EXPECT_EQ(releases.size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            releaseFeedback1.get().modify()->setDateTime(dateTime);
            releaseFeedback2.get().modify()->setDateTime(dateTime.addSecs(-1));

            auto releases{ Release::findIds(session, Release::FindParameters{}.setFeedbackUser(user.getId()).setSortMethod(ReleaseSortMethod::FeedbackDateDesc)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], releaseFeedback1->getRelease()->getId());
            EXPECT_EQ(releases[1], releaseFeedback2->getRelease()->getId());
        }
        {
            auto transaction{ session.createWriteTransaction() };

            releaseFeedback1.get().modify()->setDateTime(dateTime);
            releaseFeedback2.get().modify()->setDateTime(dateTime.addSecs(1));

            auto releases{ Release::findIds(session, Release::FindParameters{}.setFeedbackUser(user.getId()).setSortMethod(ReleaseSortMethod::FeedbackDateDesc)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], releaseFeedback2->getRelease()->getId());
            EXPECT_EQ(releases[1], releaseFeedback1->getRelease()->getId());
        }
    }
} // namespace lms::db::tests
