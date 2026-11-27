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

#include "database/objects/PlayQueue.hpp"
#include "database/objects/TrackList.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    TEST_F(DatabaseFixture, PlayQueue_createDestroy)
    {
        ScopedUser user{ session, "MyUser" };

        PlayQueueId playQueueId;
        TrackListId trackListId;
        {
            auto transaction{ session.createWriteTransaction() };

            const PlayQueue playQueue{ PlayQueue::create(session, user.getId(), PlayQueueType::UI) };
            playQueueId = playQueue.getId();
            trackListId = playQueue.getTrackListId();
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(PlayQueue::getCount(session), 1);

            const std::optional<PlayQueue> playQueue{ PlayQueue::find(session, playQueueId) };
            ASSERT_TRUE(playQueue);
            EXPECT_EQ(playQueue->getType(), PlayQueueType::UI);
            EXPECT_EQ(playQueue->getUserId(), user.getId());
            EXPECT_EQ(playQueue->getCurrentIndex(), 0);

            const std::optional<TrackList> trackList{ TrackList::find(session, trackListId) };
            ASSERT_TRUE(trackList);
            EXPECT_EQ(trackList->getType(), TrackListType::Internal);
            EXPECT_EQ(trackList->getUserId(), user.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            PlayQueue::destroy(session, std::span{ &playQueueId, 1 });
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(PlayQueue::getCount(session), 0);
            EXPECT_FALSE(TrackList::find(session, trackListId));
        }
    }

    TEST_F(DatabaseFixture, PlayQueue_findByUserAndType)
    {
        ScopedUser user1{ session, "MyUser1" };
        ScopedUser user2{ session, "MyUser2" };
        ScopedPlayQueue uiPlayQueue{ session, user1.getId(), PlayQueueType::UI };
        ScopedPlayQueue subsonicPlayQueue{ session, user1.getId(), PlayQueueType::Subsonic };

        {
            auto transaction{ session.createReadTransaction() };

            const std::optional<PlayQueue> found{ PlayQueue::find(session, user1.getId(), PlayQueueType::UI) };
            ASSERT_TRUE(found);
            EXPECT_EQ(found->getId(), uiPlayQueue.getId());

            const std::optional<PlayQueue> foundSubsonic{ PlayQueue::find(session, user1.getId(), PlayQueueType::Subsonic) };
            ASSERT_TRUE(foundSubsonic);
            EXPECT_EQ(foundSubsonic->getId(), subsonicPlayQueue.getId());

            EXPECT_FALSE(PlayQueue::find(session, user2.getId(), PlayQueueType::UI));

            const std::vector<PlayQueueId> ids{ PlayQueue::findIds(session, user1.getId()) };
            ASSERT_EQ(ids.size(), 2);
            EXPECT_EQ(ids[0], uiPlayQueue.getId());
            EXPECT_EQ(ids[1], subsonicPlayQueue.getId());

            EXPECT_TRUE(PlayQueue::findIds(session, user2.getId()).empty());
        }
    }

    TEST_F(DatabaseFixture, PlayQueue_update)
    {
        ScopedUser user{ session, "MyUser" };
        ScopedPlayQueue playQueue{ session, user.getId(), PlayQueueType::Subsonic };

        const Wt::WDateTime dateTime{ Wt::WDate{ 2026, 11, 11 }, Wt::WTime{ 12, 30 } };
        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<PlayQueue> value{ PlayQueue::find(session, playQueue.getId()) };
            ASSERT_TRUE(value);
            value->setCurrentIndex(3);
            value->setCurrentPositionInTrack(std::chrono::milliseconds{ 1500 });
            value->setLastModifiedDateTime(dateTime);
            PlayQueue::update(session, *value);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const std::optional<PlayQueue> value{ PlayQueue::find(session, playQueue.getId()) };
            ASSERT_TRUE(value);
            EXPECT_EQ(value->getCurrentIndex(), 3);
            EXPECT_EQ(value->getCurrentPositionInTrack(), std::chrono::milliseconds{ 1500 });
            EXPECT_EQ(value->getLastModifiedDateTime(), dateTime);
        }
    }
} // namespace lms::db::tests
