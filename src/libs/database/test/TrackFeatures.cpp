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

#include "Common.hpp"

#include "database/objects/TrackFeatures.hpp"

namespace lms::db::tests
{
    using ScopedTrackFeatures = ScopedEntity<db::TrackFeatures>;

    TEST_F(DatabaseFixture, TrackFeatures)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackFeatures::getCount(session), 0);
        }

        ScopedTrackFeatures trackFeatures{ session, track.lockAndGet(), "" };

        {
            auto transaction{ session.createWriteTransaction() };
            EXPECT_EQ(TrackFeatures::getCount(session), 1);

            auto allTrackFeatures{ TrackFeatures::find(session) };
            ASSERT_EQ(allTrackFeatures.results.size(), 1);
            EXPECT_EQ(allTrackFeatures.results.front(), trackFeatures.getId());
        }
    }
} // namespace lms::db::tests