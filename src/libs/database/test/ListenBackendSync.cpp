/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <optional>

#include "database/objects/Listen.hpp"
#include "database/objects/ListenBackendSync.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedListen = ScopedEntity<db::Listen>;
    using ScopedListenBackendSync = ScopedEntity<db::ListenBackendSync>;

    TEST_F(DatabaseFixture, ListenBackendSync_create)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } } };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(ListenBackendSync::getCount(session), 0);
            EXPECT_FALSE(ListenBackendSync::find(session, listen.getId(), ScrobblingBackend::ListenBrainz));
        }

        ScopedListenBackendSync sync{ session, listen.lockAndGet(), ScrobblingBackend::ListenBrainz };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(ListenBackendSync::getCount(session), 1);

            const auto found{ ListenBackendSync::find(session, listen.getId(), ScrobblingBackend::ListenBrainz) };
            ASSERT_TRUE(found);
            EXPECT_EQ(found->getId(), sync.getId());
            EXPECT_EQ(found->getBackend(), ScrobblingBackend::ListenBrainz);
            EXPECT_EQ(found->getSyncState(), SyncState::PendingAdd);
            EXPECT_EQ(found->getListen()->getId(), listen.getId());

            // no sync row exists for a backend that was never enqueued
            EXPECT_FALSE(ListenBackendSync::find(session, listen.getId(), ScrobblingBackend::LastFm));
        }
    }

    TEST_F(DatabaseFixture, ListenBackendSync_findByParameters)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } } };
        ScopedListen listen2{ session, user.lockAndGet(), track.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 13, 0, 1 } } };

        ScopedListenBackendSync sync1{ session, listen1.lockAndGet(), ScrobblingBackend::ListenBrainz };
        ScopedListenBackendSync sync2{ session, listen2.lockAndGet(), ScrobblingBackend::ListenBrainz };
        {
            auto transaction{ session.createWriteTransaction() };
            sync2.get().modify()->setSyncState(SyncState::Synchronized);
        }
        ScopedListenBackendSync sync3{ session, listen1.lockAndGet(), ScrobblingBackend::LastFm };

        {
            auto transaction{ session.createReadTransaction() };

            auto pendingListenBrainz{ ListenBackendSync::find(session, ListenBackendSync::FindParameters{}.setBackend(ScrobblingBackend::ListenBrainz).setSyncState(SyncState::PendingAdd)) };
            ASSERT_EQ(pendingListenBrainz.size(), 1);
            EXPECT_EQ(pendingListenBrainz.front(), sync1.getId());

            auto allListenBrainz{ ListenBackendSync::find(session, ListenBackendSync::FindParameters{}.setBackend(ScrobblingBackend::ListenBrainz)) };
            EXPECT_EQ(allListenBrainz.size(), 2);

            auto allLastFm{ ListenBackendSync::find(session, ListenBackendSync::FindParameters{}.setBackend(ScrobblingBackend::LastFm)) };
            ASSERT_EQ(allLastFm.size(), 1);
            EXPECT_EQ(allLastFm.front(), sync3.getId());
        }
    }

    TEST_F(DatabaseFixture, ListenBackendSync_cascadeDeleteOnListen)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        std::optional<ScopedListen> listen{ std::in_place, session, user.lockAndGet(), track.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } } };
        ScopedListenBackendSync sync{ session, listen->lockAndGet(), ScrobblingBackend::ListenBrainz };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(ListenBackendSync::getCount(session), 1);
        }

        listen.reset();

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(ListenBackendSync::getCount(session), 0);
        }
    }
} // namespace lms::db::tests
