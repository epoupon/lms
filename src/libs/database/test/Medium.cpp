/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "database/Types.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Track.hpp"

namespace lms::db::tests
{
    using ScopedMedium = ScopedEntity<db::Medium>;

    TEST_F(DatabaseFixture, Medium)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Medium::getCount(session), 0);

            Medium::pointer medium{ Medium::find(session, MediumId{}) };
            ASSERT_EQ(medium, Medium::pointer{});
        }

        ScopedMedium medium{ session, release.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Medium::getCount(session), 1);

            Medium::pointer foundMedium{ Medium::find(session, medium.getId()) };
            ASSERT_NE(foundMedium, Medium::pointer{});
            EXPECT_EQ(foundMedium->getReleaseId(), release.getId());
            EXPECT_EQ(foundMedium->getPosition(), std::nullopt);
            EXPECT_EQ(foundMedium->getMedia(), "");
            EXPECT_EQ(foundMedium->getName(), "");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            medium.get().modify()->setName("MySubtitle");
            medium.get().modify()->setPosition(1);
            medium.get().modify()->setMedia("MyMedia");
        }

        {
            auto transaction{ session.createReadTransaction() };

            Medium::pointer foundMedium{ Medium::find(session, medium.getId()) };
            ASSERT_NE(foundMedium, Medium::pointer{});
            EXPECT_EQ(foundMedium->getName(), "MySubtitle");
            EXPECT_EQ(foundMedium->getPosition(), 1);
            EXPECT_EQ(foundMedium->getMedia(), "MyMedia");
        }
    }

    TEST_F(DatabaseFixture, MediumFindByRelease_noPosition)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };

            Medium::pointer foundMedium{ Medium::find(session, release.getId(), std::nullopt) };
            ASSERT_EQ(foundMedium, Medium::pointer{});
        }

        ScopedMedium medium{ session, release.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };

            Medium::pointer foundMedium{ Medium::find(session, release.getId(), std::nullopt) };
            ASSERT_NE(foundMedium, Medium::pointer{});
            EXPECT_EQ(foundMedium->getId(), medium.getId());
        }
    }

    TEST_F(DatabaseFixture, MediumFindByRelease_withPosition)
    {
        ScopedRelease release{ session, "MyRelease" };

        ScopedMedium medium{ session, release.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };

            Medium::pointer foundMedium{ Medium::find(session, release.getId(), 1) };
            ASSERT_EQ(foundMedium, Medium::pointer{});
        }

        {
            auto transaction{ session.createWriteTransaction() };

            medium.get().modify()->setPosition(1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Medium::pointer foundMedium{ Medium::find(session, release.getId(), 1) };
            ASSERT_NE(foundMedium, Medium::pointer{});
            EXPECT_EQ(foundMedium->getId(), medium.getId());
        }
    }

    TEST_F(DatabaseFixture, Medium_findTracksByMedium)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedMedium medium{ session, release.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setMedium(medium.getId());
            params.setSortMethod(db::TrackSortMethod::TrackNumber);

            bool visited{};
            db::Track::find(session, params, [&](const db::Track::pointer&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setRelease(release.get());
            track2.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setMedium(medium.getId());
            params.setSortMethod(db::TrackSortMethod::TrackNumber);

            bool visited{};
            db::Track::find(session, params, [&](const db::Track::pointer&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setMedium(medium.get());
            track2.get().modify()->setMedium(medium.get());

            track1.get().modify()->setTrackNumber(3);
            track2.get().modify()->setTrackNumber(1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setMedium(medium.getId());
            params.setSortMethod(db::TrackSortMethod::TrackNumber);

            std::vector<db::TrackId> visitedTrackIds;
            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                visitedTrackIds.push_back(track->getId());
            });
            ASSERT_EQ(visitedTrackIds.size(), 2);
            EXPECT_EQ(visitedTrackIds[0], track2.getId());
            EXPECT_EQ(visitedTrackIds[1], track1.getId());
        }
    }

} // namespace lms::db::tests