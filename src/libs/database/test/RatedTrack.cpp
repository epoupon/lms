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

#include "database/objects/RatedTrack.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedRatedTrack = ScopedEntity<db::RatedTrack>;

    TEST_F(DatabaseFixture, RatedTrack)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto starredTrack{ RatedTrack::find(session, track->getId(), user->getId()) };
            EXPECT_FALSE(starredTrack);
            EXPECT_EQ(RatedTrack::getCount(session), 0);

            auto tracks{ Track::findIds(session, Track::FindParameters{}) };
            EXPECT_EQ(tracks.results.size(), 1);
        }

        ScopedRatedTrack ratedTrack{ session, track.lockAndGet(), user.lockAndGet() };
        {
            auto transaction{ session.createReadTransaction() };

            auto gotTrack{ RatedTrack::find(session, track->getId(), user->getId()) };
            EXPECT_EQ(gotTrack->getId(), ratedTrack->getId());
            EXPECT_EQ(gotTrack->getRating(), 0);
            EXPECT_EQ(RatedTrack::getCount(session), 1);
        }
    }
} // namespace lms::db::tests