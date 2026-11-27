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

            EXPECT_FALSE(Medium::find(session, MediumId{}));
        }

        ScopedMedium medium{ session, release.getId() };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Medium::getCount(session), 1);

            std::optional<Medium> foundMedium{ Medium::find(session, medium.getId()) };
            ASSERT_TRUE(foundMedium);
            EXPECT_EQ(foundMedium->getReleaseId(), release.getId());
            EXPECT_EQ(foundMedium->getPosition(), std::nullopt);
            EXPECT_EQ(foundMedium->getMedia(), "");
            EXPECT_EQ(foundMedium->getName(), "");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Medium> updatedMedium{ medium.get() };
            updatedMedium->setName("MySubtitle");
            updatedMedium->setPosition(1);
            updatedMedium->setMedia("MyMedia");
            Medium::update(session, *updatedMedium);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Medium> foundMedium{ Medium::find(session, medium.getId()) };
            ASSERT_TRUE(foundMedium);
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

            std::optional<Medium> foundMedium{ Medium::find(session, release.getId(), std::nullopt) };
            ASSERT_FALSE(foundMedium);
        }

        ScopedMedium medium{ session, release.getId() };

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Medium> foundMedium{ Medium::find(session, release.getId(), std::nullopt) };
            ASSERT_TRUE(foundMedium);
            EXPECT_EQ(foundMedium->getId(), medium.getId());
        }
    }

    TEST_F(DatabaseFixture, MediumFindByRelease_withPosition)
    {
        ScopedRelease release{ session, "MyRelease" };

        ScopedMedium medium{ session, release.getId() };

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Medium> foundMedium{ Medium::find(session, release.getId(), 1) };
            ASSERT_FALSE(foundMedium);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Medium> updatedMedium{ medium.get() };
            updatedMedium->setPosition(1);
            Medium::update(session, *updatedMedium);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Medium> foundMedium{ Medium::find(session, release.getId(), 1) };
            ASSERT_TRUE(foundMedium);
            EXPECT_EQ(foundMedium->getId(), medium.getId());
        }
    }

    TEST_F(DatabaseFixture, Medium_findTracksByMedium)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedMedium medium{ session, release.getId() };

        {
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setMedium(medium.getId());
            params.setSortMethod(db::TrackSortMethod::TrackNumber);

            bool visited{};
            db::Track::find(session, params, [&](const db::Track&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release.getId());
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setMedium(medium.getId());
            params.setSortMethod(db::TrackSortMethod::TrackNumber);

            bool visited{};
            db::Track::find(session, params, [&](const db::Track&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setMedium(medium.getId());
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setMedium(medium.getId());
            Track::update(session, *updatedTrack2);

            updatedTrack1 = track1.get();
            updatedTrack1->setTrackNumber(3);
            Track::update(session, *updatedTrack1);
            updatedTrack2 = track2.get();
            updatedTrack2->setTrackNumber(1);
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setMedium(medium.getId());
            params.setSortMethod(db::TrackSortMethod::TrackNumber);

            std::vector<db::TrackId> visitedTrackIds;
            db::Track::find(session, params, [&](const db::Track& track) {
                visitedTrackIds.push_back(track.getId());
            });
            ASSERT_EQ(visitedTrackIds.size(), 2);
            EXPECT_EQ(visitedTrackIds[0], track2.getId());
            EXPECT_EQ(visitedTrackIds[1], track1.getId());
        }
    }

} // namespace lms::db::tests