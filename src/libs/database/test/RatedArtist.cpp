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

#include "database/RatedArtist.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedRatedArtist = ScopedEntity<db::RatedArtist>;

    TEST_F(DatabaseFixture, RatedArtist)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto starredArtist{ RatedArtist::find(session, artist->getId(), user->getId()) };
            EXPECT_FALSE(starredArtist);
            EXPECT_EQ(RatedArtist::getCount(session), 0);

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            EXPECT_EQ(artists.results.size(), 1);
        }

        ScopedRatedArtist ratedArtist{ session, artist.lockAndGet(), user.lockAndGet() };
        {
            auto transaction{ session.createReadTransaction() };

            auto gotArtist{ RatedArtist::find(session, artist->getId(), user->getId()) };
            EXPECT_EQ(gotArtist->getId(), ratedArtist->getId());
            EXPECT_EQ(gotArtist->getRating(), 0);
            EXPECT_EQ(RatedArtist::getCount(session), 1);
        }
    }
} // namespace lms::db::tests