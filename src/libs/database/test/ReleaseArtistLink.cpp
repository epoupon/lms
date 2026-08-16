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

            ReleaseArtistLink::FindParameters params;
            params.setRelease(release.getId());

            bool visited{};
            ReleaseArtistLink::find(session, params, [&](const ReleaseArtistLink::pointer&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setRelease(release.get());
            session.create<ReleaseArtistLink>(release.get(), artist.get(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseArtistLink::FindParameters params;
            params.setRelease(release.getId());
            params.setMBIDMatched(false);

            bool visited{};
            ReleaseArtistLink::find(session, params, [&](const ReleaseArtistLink::pointer& link) {
                visited = true;
                EXPECT_EQ(link->getArtistId(), artist.getId());
            });
            EXPECT_TRUE(visited);
        }
    }

    TEST_F(DatabaseFixture, ReleaseArtistLink_findWithSortNameNotEmpty)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        {
            auto transaction{ session.createWriteTransaction() };
            session.create<ReleaseArtistLink>(release1.get(), artist.get(), false);
            // release2's link has no sort name set at all
            session.create<ReleaseArtistLink>(release2.get(), artist.get(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseArtistLink::FindParameters params;
            params.setSortNameNotEmpty(true);

            std::vector<ReleaseArtistLink::pointer> links;
            ReleaseArtistLink::find(session, params, [&](const ReleaseArtistLink::pointer& link) {
                links.push_back(link);
            });
            ASSERT_EQ(links.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            ReleaseArtistLink::FindParameters params;
            ReleaseArtistLink::find(session, params, [&](ReleaseArtistLink::pointer link) {
                if (link->getRelease()->getId() == release1.getId())
                    link.modify()->setArtistSortName("MyArtist, Sort");
            });
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseArtistLink::FindParameters params;
            params.setSortNameNotEmpty(true);

            std::vector<ReleaseArtistLink::pointer> links;
            ReleaseArtistLink::find(session, params, [&](const ReleaseArtistLink::pointer& link) {
                links.push_back(link);
            });
            ASSERT_EQ(links.size(), 1);
            EXPECT_EQ(links[0]->getRelease()->getId(), release1.getId());
        }
    }
} // namespace lms::db::tests