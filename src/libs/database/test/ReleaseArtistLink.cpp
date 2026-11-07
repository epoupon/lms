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

#include "database/objects/ReleaseArtistLink.hpp"

namespace lms::db::tests
{
    TEST_F(DatabaseFixture, ReleaseArtistLink)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_TRUE(release->getArtistLinks().empty());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setRelease(release.get());
            ReleaseArtistLink::create(session, release.get(), artist.get(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const std::vector<ReleaseArtistLink::pointer> links{ release->getArtistLinks() };
            ASSERT_EQ(links.size(), 1);
            EXPECT_EQ(links[0]->getArtistId(), artist.getId());
        }
    }
} // namespace lms::db::tests