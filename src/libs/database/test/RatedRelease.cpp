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

#include "database/objects/RatedRelease.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedRatedRelease = ScopedEntity<db::RatedRelease>;

    TEST_F(DatabaseFixture, RatedRelease)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto starredRelease{ RatedRelease::find(session, release->getId(), user->getId()) };
            EXPECT_FALSE(starredRelease);
            EXPECT_EQ(RatedRelease::getCount(session), 0);

            auto releases{ Release::findIds(session, Release::FindParameters{}) };
            EXPECT_EQ(releases.results.size(), 1);
        }

        ScopedRatedRelease ratedRelease{ session, release.lockAndGet(), user.lockAndGet() };
        {
            auto transaction{ session.createReadTransaction() };

            auto gotRelease{ RatedRelease::find(session, release->getId(), user->getId()) };
            EXPECT_EQ(gotRelease->getId(), ratedRelease->getId());
            EXPECT_EQ(gotRelease->getRating(), 0);
            EXPECT_EQ(RatedRelease::getCount(session), 1);
        }
    }
} // namespace lms::db::tests