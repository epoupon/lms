/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "database/objects/Work.hpp"

namespace lms::db::tests
{
    using ScopedWork = ScopedEntity<db::Work>;

    TEST_F(DatabaseFixture, Work_create)
    {
        ScopedWork work{ session, "Symphony No. 5", std::optional<core::UUID>{} };

        {
            auto transaction{ session.createReadTransaction() };

            const Work::pointer found{ Work::find(session, work.getId()) };
            ASSERT_TRUE(found);
            EXPECT_EQ(found->getName(), "Symphony No. 5");
            EXPECT_FALSE(found->getMBID());
        }
    }

    TEST_F(DatabaseFixture, Work_createWithMBID)
    {
        const auto mbid{ core::UUID::fromString("8f3471b3-7e93-4de1-a9c7-3b843c21b84e") };
        ASSERT_TRUE(mbid);

        ScopedWork work{ session, "Symphony No. 9", mbid };

        {
            auto transaction{ session.createReadTransaction() };

            const Work::pointer found{ Work::find(session, work.getId()) };
            ASSERT_TRUE(found);
            EXPECT_EQ(found->getName(), "Symphony No. 9");
            ASSERT_TRUE(found->getMBID());
            EXPECT_EQ(found->getMBID(), mbid);
        }
    }

    TEST_F(DatabaseFixture, Work_findByMBID)
    {
        const auto mbid{ core::UUID::fromString("8f3471b3-7e93-4de1-a9c7-3b843c21b84e") };
        ASSERT_TRUE(mbid);

        ScopedWork workWithMbid{ session, "Piano Sonata", mbid };

        {
            auto transaction{ session.createReadTransaction() };

            const Work::pointer byMbid{ Work::find(session, *mbid) };
            ASSERT_TRUE(byMbid);
            EXPECT_EQ(byMbid->getId(), workWithMbid.getId());
        }
    }

    TEST_F(DatabaseFixture, Work_findByNameScopedToRelease)
    {
        // Work titles are often generic (e.g. "Piano Sonata"): two unrelated releases can each have
        // their own work with the exact same name, and lookup must not merge them
        ScopedRelease release1{ session, "Release1" };
        ScopedRelease release2{ session, "Release2" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedWork work1{ session, "Piano Sonata", std::optional<core::UUID>{} };
        ScopedWork work2{ session, "Piano Sonata", std::optional<core::UUID>{} };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setRelease(release1.get());
            track1.get().modify()->setWorks(std::array{ work1.get() });
            track2.get().modify()->setRelease(release2.get());
            track2.get().modify()->setWorks(std::array{ work2.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };

            // Same name, but each release resolves to its own work
            const Work::pointer foundInRelease1{ Work::find(session, release1.getId(), "Piano Sonata") };
            ASSERT_TRUE(foundInRelease1);
            EXPECT_EQ(foundInRelease1->getId(), work1.getId());

            const Work::pointer foundInRelease2{ Work::find(session, release2.getId(), "Piano Sonata") };
            ASSERT_TRUE(foundInRelease2);
            EXPECT_EQ(foundInRelease2->getId(), work2.getId());

            // Unknown name in a known release returns null
            EXPECT_FALSE(Work::find(session, release1.getId(), "Unknown Work"));
        }
    }

    TEST_F(DatabaseFixture, Work_orphan)
    {
        ScopedWork work{ session, "Unlinked Work", std::optional<core::UUID>{} };

        {
            auto transaction{ session.createReadTransaction() };
            const auto orphans{ Work::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), work.getId());
        }
    }

    TEST_F(DatabaseFixture, Work_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedWork work1{ session, "Requiem", std::optional<core::UUID>{} };
        ScopedWork work2{ session, "Missa Solemnis", std::optional<core::UUID>{} };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Work::findOrphanIds(session).size(), 2);
            EXPECT_EQ(track->getWorks().size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setWorks(std::array{ work1.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto orphans{ Work::findOrphanIds(session) };
            ASSERT_EQ(orphans.size(), 1);
            EXPECT_EQ(orphans.front(), work2.getId());

            const auto trackWorks{ track->getWorks() };
            ASSERT_EQ(trackWorks.size(), 1);
            EXPECT_EQ(trackWorks.front()->getId(), work1.getId());
        }
    }

    TEST_F(DatabaseFixture, Work_multipleWorksOnTrack)
    {
        ScopedTrack track{ session };
        ScopedWork work1{ session, "Symphony No. 5", std::optional<core::UUID>{} };
        ScopedWork work2{ session, "Symphony No. 6", std::optional<core::UUID>{} };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setWorks(std::array{ work1.get(), work2.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Work::findOrphanIds(session).size(), 0);

            const auto trackWorks{ track->getWorks() };
            EXPECT_EQ(trackWorks.size(), 2);
        }
    }

    TEST_F(DatabaseFixture, Work_multipleTracksOnWork)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedWork work{ session, "The Four Seasons", std::optional<core::UUID>{} };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setWorks(std::array{ work.get() });
            track2.get().modify()->setWorks(std::array{ work.get() });
            track3.get().modify()->setWorks(std::array{ work.get() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Work::findOrphanIds(session).size(), 0);

            EXPECT_EQ(track1->getWorks().size(), 1);
            EXPECT_EQ(track2->getWorks().size(), 1);
            EXPECT_EQ(track3->getWorks().size(), 1);
        }
    }

    TEST_F(DatabaseFixture, Work_nameTruncation)
    {
        const std::string longName(Work::maxNameLength + 100, 'x');
        const std::string expectedName(Work::maxNameLength, 'x');

        {
            auto transaction{ session.createWriteTransaction() };
            Work::pointer work{ session.create<Work>(longName, std::optional<core::UUID>{}) };
            ASSERT_TRUE(work);
            EXPECT_EQ(work->getName().size(), Work::maxNameLength);
            EXPECT_EQ(work->getName(), expectedName);
            work.remove();
        }
    }

} // namespace lms::db::tests
