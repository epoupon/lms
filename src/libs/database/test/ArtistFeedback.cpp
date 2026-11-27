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

#include "database/objects/ArtistFeedback.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedArtistFeedback = ScopedEntity<db::ArtistFeedback>;

    TEST_F(DatabaseFixture, ArtistFeedback)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto artistFeedback{ ArtistFeedback::find(session, artist->getId(), user->getId()) };
            EXPECT_FALSE(artistFeedback);
            EXPECT_EQ(ArtistFeedback::getCount(session), 0);

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            EXPECT_EQ(artists.size(), 1);
        }

        ScopedArtistFeedback artistFeedback{ session, artist.getId(), user.getId() };
        {
            auto transaction{ session.createWriteTransaction() };

            auto gotArtist{ ArtistFeedback::find(session, artist->getId(), user->getId()) };
            EXPECT_EQ(gotArtist->getId(), artistFeedback->getId());
            EXPECT_EQ(ArtistFeedback::getCount(session), 1);
            EXPECT_EQ(gotArtist->getValue(), FeedbackValue::None);

            std::optional<ArtistFeedback> updatedArtistFeedback{ artistFeedback.get() };
            updatedArtistFeedback->setValue(FeedbackValue::Loved);
            ArtistFeedback::update(session, *updatedArtistFeedback);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}) };
            EXPECT_EQ(artists.size(), 1);

            artists = Artist::findIds(session, Artist::FindParameters{}.setFeedbackUser(user.getId()));
            EXPECT_EQ(artists.size(), 1);

            artists = Artist::findIds(session, Artist::FindParameters{}.setFeedbackUser(user2.getId()));
            EXPECT_EQ(artists.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, ArtistFeedback_ClearedValueExcluded)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        ScopedArtistFeedback artistFeedback{ session, artist.getId(), user.getId() };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<ArtistFeedback> updatedArtistFeedback{ artistFeedback.get() };
            updatedArtistFeedback->setValue(FeedbackValue::Loved);
            ArtistFeedback::update(session, *updatedArtistFeedback);
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved)) };
            EXPECT_EQ(artists.size(), 1);

            updatedArtistFeedback = artistFeedback.get();
            updatedArtistFeedback->setValue(FeedbackValue::None);
            ArtistFeedback::update(session, *updatedArtistFeedback);
            artists = Artist::findIds(session, Artist::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved));
            EXPECT_EQ(artists.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, ArtistFeedback_HatedExcluded)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        ScopedArtistFeedback artistFeedback{ session, artist.getId(), user.getId() };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<ArtistFeedback> updatedArtistFeedback{ artistFeedback.get() };
            updatedArtistFeedback->setValue(FeedbackValue::Hated);
            ArtistFeedback::update(session, *updatedArtistFeedback);
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved)) };
            EXPECT_EQ(artists.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, ArtistFeedback_AnyValueIncludedWhenValueNotSet)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        ScopedArtistFeedback artistFeedback{ session, artist.getId(), user.getId() };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<ArtistFeedback> updatedArtistFeedback{ artistFeedback.get() };
            updatedArtistFeedback->setValue(FeedbackValue::Hated);
            ArtistFeedback::update(session, *updatedArtistFeedback);
            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFeedbackUser(user.getId())) };
            EXPECT_EQ(artists.size(), 1);
        }
    }

    TEST_F(DatabaseFixture, ArtistFeedback_ValueFilterAcrossUsers)
    {
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedArtist artist2{ session, "MyArtist2" };
        ScopedUser user1{ session, "MyUser1" };
        ScopedUser user2{ session, "MyUser2" };
        ScopedArtistFeedback artistFeedback1{ session, artist1.getId(), user1.getId() };
        ScopedArtistFeedback artistFeedback2{ session, artist2.getId(), user2.getId() };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<ArtistFeedback> updatedArtistFeedback1{ artistFeedback1.get() };
            updatedArtistFeedback1->setValue(FeedbackValue::Loved);
            ArtistFeedback::update(session, *updatedArtistFeedback1);
            std::optional<ArtistFeedback> updatedArtistFeedback2{ artistFeedback2.get() };
            updatedArtistFeedback2->setValue(FeedbackValue::Hated);
            ArtistFeedback::update(session, *updatedArtistFeedback2);

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFeedbackValue(FeedbackValue::Loved)) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist1->getId());
        }
    }

    TEST_F(DatabaseFixture, ArtistFeedback_dateTime)
    {
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedArtist artist2{ session, "MyArtist2" };
        ScopedUser user{ session, "MyUser" };

        ScopedArtistFeedback artistFeedback1{ session, artist1.getId(), user.getId() };
        ScopedArtistFeedback artistFeedback2{ session, artist2.getId(), user.getId() };

        const Wt::WDateTime dateTime{ Wt::WDate{ 1950, 1, 2 }, Wt::WTime{ 12, 30, 1 } };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<ArtistFeedback> updatedArtistFeedback1{ artistFeedback1.get() };
            updatedArtistFeedback1->setValue(FeedbackValue::Loved);
            ArtistFeedback::update(session, *updatedArtistFeedback1);
            std::optional<ArtistFeedback> updatedArtistFeedback2{ artistFeedback2.get() };
            updatedArtistFeedback2->setValue(FeedbackValue::Loved);
            ArtistFeedback::update(session, *updatedArtistFeedback2);

            auto artists{ Artist::find(session, Artist::FindParameters{}.setFeedbackUser(user.getId())) };
            EXPECT_EQ(artists.size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<ArtistFeedback> updatedArtistFeedback1{ artistFeedback1.get() };
            updatedArtistFeedback1->setDateTime(dateTime);
            ArtistFeedback::update(session, *updatedArtistFeedback1);
            std::optional<ArtistFeedback> updatedArtistFeedback2{ artistFeedback2.get() };
            updatedArtistFeedback2->setDateTime(dateTime.addSecs(-1));
            ArtistFeedback::update(session, *updatedArtistFeedback2);

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFeedbackUser(user.getId()).setSortMethod(ArtistSortMethod::FeedbackDateDesc)) };
            ASSERT_EQ(artists.size(), 2);
            EXPECT_EQ(artists[0], artistFeedback1->getArtistId());
            EXPECT_EQ(artists[1], artistFeedback2->getArtistId());
        }
        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<ArtistFeedback> updatedArtistFeedback1{ artistFeedback1.get() };
            updatedArtistFeedback1->setDateTime(dateTime);
            ArtistFeedback::update(session, *updatedArtistFeedback1);
            std::optional<ArtistFeedback> updatedArtistFeedback2{ artistFeedback2.get() };
            updatedArtistFeedback2->setDateTime(dateTime.addSecs(1));
            ArtistFeedback::update(session, *updatedArtistFeedback2);

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFeedbackUser(user.getId()).setSortMethod(ArtistSortMethod::FeedbackDateDesc)) };
            ASSERT_EQ(artists.size(), 2);
            EXPECT_EQ(artists[0], artistFeedback2->getArtistId());
            EXPECT_EQ(artists[1], artistFeedback1->getArtistId());
        }
    }
} // namespace lms::db::tests
