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

#include "database/objects/TrackFeedback.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedTrackFeedback = ScopedEntity<db::TrackFeedback>;

    TEST_F(DatabaseFixture, TrackFeedback)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            auto trackFeedback{ TrackFeedback::find(session, track->getId(), user->getId()) };
            EXPECT_FALSE(trackFeedback);
            EXPECT_EQ(TrackFeedback::getCount(session), 0);

            auto tracks{ Track::findIds(session, Track::FindParameters{}) };
            EXPECT_EQ(tracks.size(), 1);
        }

        ScopedTrackFeedback trackFeedback{ session, track.lockAndGet(), user.lockAndGet() };
        {
            auto transaction{ session.createWriteTransaction() };

            auto gotTrack{ TrackFeedback::find(session, track->getId(), user->getId()) };
            EXPECT_EQ(gotTrack->getId(), trackFeedback->getId());
            EXPECT_EQ(TrackFeedback::getCount(session), 1);
            EXPECT_EQ(gotTrack->getValue(), FeedbackValue::None);

            trackFeedback.get().modify()->setValue(FeedbackValue::Loved);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}) };
            EXPECT_EQ(tracks.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user.getId()));
            EXPECT_EQ(tracks.size(), 1);

            tracks = Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user2.getId()));
            EXPECT_EQ(tracks.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, TrackFeedback_ClearedValueExcluded)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedTrackFeedback trackFeedback{ session, track.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback.get().modify()->setValue(FeedbackValue::Loved);
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved)) };
            EXPECT_EQ(tracks.size(), 1);

            trackFeedback.get().modify()->setValue(FeedbackValue::None);
            tracks = Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved));
            EXPECT_EQ(tracks.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, TrackFeedback_HatedExcluded)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedTrackFeedback trackFeedback{ session, track.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback.get().modify()->setValue(FeedbackValue::Hated);
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved)) };
            EXPECT_EQ(tracks.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, TrackFeedback_AnyValueIncludedWhenValueNotSet)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedTrackFeedback trackFeedback{ session, track.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback.get().modify()->setValue(FeedbackValue::Hated);
            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user.getId())) };
            EXPECT_EQ(tracks.size(), 1);
        }
    }

    TEST_F(DatabaseFixture, TrackFeedback_ValueFilterAcrossUsers)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user1{ session, "MyUser1" };
        ScopedUser user2{ session, "MyUser2" };
        ScopedTrackFeedback trackFeedback1{ session, track1.lockAndGet(), user1.lockAndGet() };
        ScopedTrackFeedback trackFeedback2{ session, track2.lockAndGet(), user2.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback1.get().modify()->setValue(FeedbackValue::Loved);
            trackFeedback2.get().modify()->setValue(FeedbackValue::Hated);

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFeedbackValue(FeedbackValue::Loved)) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track1->getId());
        }
    }

    TEST_F(DatabaseFixture, TrackFeedback_findByUser)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedTrackFeedback trackFeedback1{ session, track1.lockAndGet(), user.lockAndGet() };
        ScopedTrackFeedback trackFeedback2{ session, track2.lockAndGet(), user.lockAndGet() };
        ScopedTrackFeedback trackFeedback3{ session, track3.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback1.get().modify()->setValue(FeedbackValue::Loved);
            trackFeedback2.get().modify()->setValue(FeedbackValue::Hated);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackFeedbackId> ids;
            TrackFeedback::find(session, TrackFeedback::FindParameters{}.setUser(user.getId()).setSortMethod(TrackFeedbackSortMethod::Id), [&](const TrackFeedback::pointer& feedback) {
                ids.push_back(feedback->getId());
            });

            ASSERT_EQ(ids.size(), 3);
            EXPECT_EQ(ids[0], trackFeedback1->getId());
            EXPECT_EQ(ids[1], trackFeedback2->getId());
            EXPECT_EQ(ids[2], trackFeedback3->getId());
        }
    }

    TEST_F(DatabaseFixture, TrackFeedback_findByUser_ScopedToUser)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user1{ session, "MyUser1" };
        ScopedUser user2{ session, "MyUser2" };
        ScopedTrackFeedback trackFeedback1{ session, track1.lockAndGet(), user1.lockAndGet() };
        ScopedTrackFeedback trackFeedback2{ session, track2.lockAndGet(), user2.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback1.get().modify()->setValue(FeedbackValue::Loved);
            trackFeedback2.get().modify()->setValue(FeedbackValue::Loved);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackFeedbackId> ids;
            TrackFeedback::find(session, TrackFeedback::FindParameters{}.setUser(user1.getId()), [&](const TrackFeedback::pointer& feedback) {
                ids.push_back(feedback->getId());
            });

            ASSERT_EQ(ids.size(), 1);
            EXPECT_EQ(ids[0], trackFeedback1->getId());
        }
    }

    TEST_F(DatabaseFixture, TrackFeedback_findByUser_KeysetPagination)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedTrackFeedback trackFeedback1{ session, track1.lockAndGet(), user.lockAndGet() };
        ScopedTrackFeedback trackFeedback2{ session, track2.lockAndGet(), user.lockAndGet() };
        ScopedTrackFeedback trackFeedback3{ session, track3.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback1.get().modify()->setValue(FeedbackValue::Loved);
            trackFeedback2.get().modify()->setValue(FeedbackValue::Loved);
            trackFeedback3.get().modify()->setValue(FeedbackValue::Loved);
        }

        {
            auto transaction{ session.createReadTransaction() };

            TrackFeedbackId lastRetrievedId;

            std::vector<TrackFeedbackId> firstPage;
            TrackFeedback::find(session, TrackFeedback::FindParameters{}.setUser(user.getId()).setLastRetrievedId(lastRetrievedId).setRange(Range{ 0, 2 }).setSortMethod(TrackFeedbackSortMethod::Id), [&](const TrackFeedback::pointer& feedback) {
                lastRetrievedId = feedback->getId();
                firstPage.push_back(feedback->getId());
            });
            ASSERT_EQ(firstPage.size(), 2);
            EXPECT_EQ(firstPage[0], trackFeedback1->getId());
            EXPECT_EQ(firstPage[1], trackFeedback2->getId());

            std::vector<TrackFeedbackId> secondPage;
            TrackFeedback::find(session, TrackFeedback::FindParameters{}.setUser(user.getId()).setLastRetrievedId(lastRetrievedId).setRange(Range{ 0, 2 }).setSortMethod(TrackFeedbackSortMethod::Id), [&](const TrackFeedback::pointer& feedback) {
                lastRetrievedId = feedback->getId();
                secondPage.push_back(feedback->getId());
            });
            ASSERT_EQ(secondPage.size(), 1);
            EXPECT_EQ(secondPage[0], trackFeedback3->getId());
        }
    }

    TEST_F(DatabaseFixture, TrackFeedback_dateTime)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };

        ScopedTrackFeedback trackFeedback1{ session, track1.lockAndGet(), user.lockAndGet() };
        ScopedTrackFeedback trackFeedback2{ session, track2.lockAndGet(), user.lockAndGet() };

        const Wt::WDateTime dateTime{ Wt::WDate{ 1950, 1, 2 }, Wt::WTime{ 12, 30, 1 } };

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback1.get().modify()->setValue(FeedbackValue::Loved);
            trackFeedback2.get().modify()->setValue(FeedbackValue::Loved);

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user.getId())) };
            EXPECT_EQ(tracks.size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback1.get().modify()->setDateTime(dateTime);
            trackFeedback2.get().modify()->setDateTime(dateTime.addSecs(-1));

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user.getId()).setSortMethod(TrackSortMethod::FeedbackDateDesc)) };
            ASSERT_EQ(tracks.size(), 2);
            EXPECT_EQ(tracks[0], trackFeedback1->getTrack()->getId());
            EXPECT_EQ(tracks[1], trackFeedback2->getTrack()->getId());
        }
        {
            auto transaction{ session.createWriteTransaction() };

            trackFeedback1.get().modify()->setDateTime(dateTime);
            trackFeedback2.get().modify()->setDateTime(dateTime.addSecs(1));

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFeedbackUser(user.getId()).setSortMethod(TrackSortMethod::FeedbackDateDesc)) };
            ASSERT_EQ(tracks.size(), 2);
            EXPECT_EQ(tracks[0], trackFeedback2->getTrack()->getId());
            EXPECT_EQ(tracks[1], trackFeedback1->getTrack()->getId());
        }
    }
} // namespace lms::db::tests
