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

#include "database/objects/StarredArtist.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedStarredArtist = ScopedEntity<db::StarredArtist>;

    TEST_F(DatabaseFixture, StarredArtist)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto starredArtist{ StarredArtist::find(session, artist->getId(), user->getId(), FeedbackBackend::Internal) };
            EXPECT_FALSE(starredArtist);
            EXPECT_EQ(StarredArtist::getCount(session), 0);

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            EXPECT_EQ(artists.results.size(), 1);
        }

        ScopedStarredArtist starredArtist{ session, artist.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };
        {
            auto transaction{ session.createReadTransaction() };

            auto gotArtist{ StarredArtist::find(session, artist->getId(), user->getId(), FeedbackBackend::Internal) };
            EXPECT_EQ(gotArtist->getId(), starredArtist->getId());
            EXPECT_EQ(StarredArtist::getCount(session), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            EXPECT_EQ(artists.results.size(), 1);

            artists = Artist::findIds(session, Artist::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(artists.results.size(), 1);

            artists = Artist::findIds(session, Artist::FindParameters{}.setStarringUser(user2.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(artists.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            user.get().modify()->setFeedbackBackend(FeedbackBackend::ListenBrainz);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto gotArtist{ StarredArtist::find(session, artist->getId(), user->getId()) };
            EXPECT_EQ(gotArtist, Artist::pointer{});
        }

        {
            auto transaction{ session.createWriteTransaction() };
            user.get().modify()->setFeedbackBackend(FeedbackBackend::Internal);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            auto gotArtist{ StarredArtist::find(session, artist->getId(), user->getId()) };
            EXPECT_EQ(gotArtist->getId(), starredArtist->getId());
        }
    }

    TEST_F(DatabaseFixture, StarredArtist_PendingDestroy)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        ScopedStarredArtist starredArtist{ session, artist.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };

        {
            auto transaction{ session.createWriteTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal)) };
            EXPECT_EQ(artists.results.size(), 1);

            starredArtist.get().modify()->setSyncState(SyncState::PendingRemove);
            artists = Artist::findIds(session, Artist::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal));
            EXPECT_EQ(artists.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, StarredArtist_dateTime)
    {
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedArtist artist2{ session, "MyArtist2" };
        ScopedUser user{ session, "MyUser" };

        ScopedStarredArtist starredArtist1{ session, artist1.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };
        ScopedStarredArtist starredArtist2{ session, artist2.lockAndGet(), user.lockAndGet(), FeedbackBackend::Internal };

        const Wt::WDateTime dateTime{ Wt::WDate{ 1950, 1, 2 }, Wt::WTime{ 12, 30, 1 } };

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::find(session, Artist::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal)) };
            EXPECT_EQ(artists.results.size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            starredArtist1.get().modify()->setDateTime(dateTime);
            starredArtist2.get().modify()->setDateTime(dateTime.addSecs(-1));

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal).setSortMethod(ArtistSortMethod::StarredDateDesc)) };
            ASSERT_EQ(artists.results.size(), 2);
            EXPECT_EQ(artists.results[0], starredArtist1->getArtist()->getId());
            EXPECT_EQ(artists.results[1], starredArtist2->getArtist()->getId());
        }
        {
            auto transaction{ session.createWriteTransaction() };

            starredArtist1.get().modify()->setDateTime(dateTime);
            starredArtist2.get().modify()->setDateTime(dateTime.addSecs(1));

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setStarringUser(user.getId(), FeedbackBackend::Internal).setSortMethod(ArtistSortMethod::StarredDateDesc)) };
            ASSERT_EQ(artists.results.size(), 2);
            EXPECT_EQ(artists.results[0], starredArtist2->getArtist()->getId());
            EXPECT_EQ(artists.results[1], starredArtist1->getArtist()->getId());
        }
    }
} // namespace lms::db::tests