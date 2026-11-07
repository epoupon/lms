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

#include <array>
#include <vector>

#include "database/objects/ArtistFeedback.hpp"
#include "database/objects/FilterExpr.hpp"
#include "database/objects/RatedTrack.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/TrackFeedback.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    namespace
    {
        using ScopedCountry = ScopedEntity<db::Country>;
        using ScopedGenre = ScopedEntity<db::Genre>;
        using ScopedLabel = ScopedEntity<db::Label>;
        using ScopedListen = ScopedEntity<db::Listen>;
        using ScopedMood = ScopedEntity<db::Mood>;
        using ScopedRatedTrack = ScopedEntity<db::RatedTrack>;
        using ScopedTrackFeedback = ScopedEntity<db::TrackFeedback>;

        std::vector<TrackId> findTrackIds(ISession& session, const FilterExpr& expr, UserId user = {})
        {
            auto transaction{ session.createReadTransaction() };
            return Track::findIds(session, Track::FindParameters{}.setFilter(expr, user).setSortMethod(TrackSortMethod::Id));
        }

        std::vector<ReleaseId> findReleaseIds(ISession& session, const FilterExpr& expr, UserId user = {})
        {
            auto transaction{ session.createReadTransaction() };
            return Release::findIds(session, Release::FindParameters{}.setFilter(expr, user).setSortMethod(ReleaseSortMethod::Id));
        }

        std::vector<ArtistId> findArtistIds(ISession& session, const FilterExpr& expr, UserId user = {})
        {
            auto transaction{ session.createReadTransaction() };
            return Artist::findIds(session, Artist::FindParameters{}.setFilter(expr, user).setSortMethod(ArtistSortMethod::Id));
        }

        FilterExpr year(FilterOperator op, std::int64_t value)
        {
            return FilterExpr::condition(IntegerCondition{ IntegerField::Year, op, value });
        }

        FilterExpr listenCount(FilterOperator op, std::int64_t value)
        {
            return FilterExpr::condition(ListenCountCondition{ op, value });
        }
    } // namespace

    TEST(FilterExpr, structure)
    {
        const FilterExpr expr{ FilterExpr::And({ year(FilterOperator::Greater, 1990),
                                                 FilterExpr::Or({ year(FilterOperator::Less, 1980), FilterExpr::Not(listenCount(FilterOperator::Greater, 0)) }) }) };

        EXPECT_EQ(expr.getType(), FilterExpr::Type::And);
        ASSERT_EQ(expr.getChildren().size(), 2);
        EXPECT_EQ(expr.getChildren()[0].getType(), FilterExpr::Type::Condition);
        EXPECT_TRUE(std::holds_alternative<IntegerCondition>(expr.getChildren()[0].getCondition()));
        EXPECT_EQ(expr.getChildren()[1].getType(), FilterExpr::Type::Or);
        EXPECT_TRUE(expr.hasPerUserCondition());
        EXPECT_FALSE(expr.getChildren()[0].hasPerUserCondition());

        const std::vector<FilterExpr> conditions{ year(FilterOperator::Equal, 1990), year(FilterOperator::Equal, 1991) };
        EXPECT_EQ(FilterExpr::Or(conditions).getChildren().size(), 2);
    }

    TEST_F(DatabaseFixture, FilterExpr_trackYear)
    {
        ScopedTrack track1985{ session };
        ScopedTrack track1995{ session };
        ScopedTrack trackNoDate{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1985.get().modify()->setDate(core::PartialDateTime{ 1985, 3, 1 });
            track1995.get().modify()->setDate(core::PartialDateTime{ 1995 });
        }

        EXPECT_EQ(findTrackIds(session, year(FilterOperator::Greater, 1990)), (std::vector{ track1995.getId() }));
        EXPECT_EQ(findTrackIds(session, year(FilterOperator::LessOrEqual, 1985)), (std::vector{ track1985.getId() }));
        EXPECT_EQ(findTrackIds(session, year(FilterOperator::Equal, 1995)), (std::vector{ track1995.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(IntegerCondition{ IntegerField::Year, FilterOperator::Between, FilterRange<std::int64_t>{ 1980, 1990 } })), (std::vector{ track1985.getId() }));

        // missing value never matches, so Not() is the exact complement
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(year(FilterOperator::Greater, 1990))), (std::vector{ track1985.getId(), trackNoDate.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_trackListenCount)
    {
        ScopedTrack trackListenedTwice{ session };
        ScopedTrack trackListenedByOther{ session };
        ScopedTrack trackNeverListened{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedUser otherUser{ session, "OtherUser" };

        const Wt::WDateTime dateTime1{ Wt::WDate{ 2020, 1, 1 }, Wt::WTime{ 12, 0, 0 } };
        const Wt::WDateTime dateTime2{ Wt::WDate{ 2021, 1, 1 }, Wt::WTime{ 12, 0, 0 } };
        ScopedListen listen1{ session, user.lockAndGet(), trackListenedTwice.lockAndGet(), dateTime1 };
        ScopedListen listen2{ session, user.lockAndGet(), trackListenedTwice.lockAndGet(), dateTime2 };
        ScopedListen listen3{ session, otherUser.lockAndGet(), trackListenedByOther.lockAndGet(), dateTime1 };

        const std::vector notListened{ trackListenedByOther.getId(), trackNeverListened.getId() };
        const std::vector listened{ trackListenedTwice.getId() };

        EXPECT_EQ(findTrackIds(session, listenCount(FilterOperator::Equal, 0), user.getId()), notListened);
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(listenCount(FilterOperator::Greater, 0)), user.getId()), notListened);
        EXPECT_EQ(findTrackIds(session, listenCount(FilterOperator::Greater, 0), user.getId()), listened);
        EXPECT_EQ(findTrackIds(session, listenCount(FilterOperator::GreaterOrEqual, 2), user.getId()), listened);
        EXPECT_TRUE(findTrackIds(session, listenCount(FilterOperator::Greater, 2), user.getId()).empty());
        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(ListenCountCondition{ FilterOperator::Between, FilterRange<std::int64_t>{ 1, 2 } }), user.getId()), listened);
        EXPECT_EQ(findTrackIds(session, listenCount(FilterOperator::Greater, 0), otherUser.getId()), (std::vector{ trackListenedByOther.getId() }));

        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(LastListenDateTimeCondition{ FilterOperator::Greater, dateTime1 }), user.getId()), listened);
        EXPECT_TRUE(findTrackIds(session, FilterExpr::condition(LastListenDateTimeCondition{ FilterOperator::Greater, dateTime2 }), user.getId()).empty());
    }

    TEST_F(DatabaseFixture, FilterExpr_trackBooleanComposition)
    {
        ScopedTrack trackRock{ session };
        ScopedTrack trackRockHappy{ session };
        ScopedTrack trackHappy{ session };
        ScopedGenre rock{ session, "Rock" };
        ScopedMood happy{ session, "Happy" };

        {
            auto transaction{ session.createWriteTransaction() };
            trackRock.get().modify()->setGenres(std::array{ rock.get() });
            trackRockHappy.get().modify()->setGenres(std::array{ rock.get() });
            trackRockHappy.get().modify()->setMoods(std::array{ happy.get() });
            trackHappy.get().modify()->setMoods(std::array{ happy.get() });
        }

        const FilterExpr isRock{ FilterExpr::equal(rock.getId()) };
        const FilterExpr isHappy{ FilterExpr::equal(happy.getId()) };

        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, isHappy })), (std::vector{ trackRockHappy.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Or({ isRock, isHappy })), (std::vector{ trackRock.getId(), trackRockHappy.getId(), trackHappy.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, FilterExpr::Not(isHappy) })), (std::vector{ trackRock.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(FilterExpr::Or({ isRock, isHappy }))), std::vector<TrackId>{});
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isHappy, FilterExpr::Or({ isRock, FilterExpr::Not(isRock) }) })), (std::vector{ trackRockHappy.getId(), trackHappy.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_trackRepeatedRelationCondition)
    {
        ScopedTrack trackRock{ session };
        ScopedTrack trackRockHappy{ session };
        ScopedTrack trackHappy{ session };
        ScopedGenre rock{ session, "Rock" };
        ScopedMood happy{ session, "Happy" };

        {
            auto transaction{ session.createWriteTransaction() };
            trackRock.get().modify()->setGenres(std::array{ rock.get() });
            trackRockHappy.get().modify()->setGenres(std::array{ rock.get() });
            trackRockHappy.get().modify()->setMoods(std::array{ happy.get() });
            trackHappy.get().modify()->setMoods(std::array{ happy.get() });
        }

        const FilterExpr isRock{ FilterExpr::equal(rock.getId()) };
        const FilterExpr isHappy{ FilterExpr::equal(happy.getId()) };

        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, isRock })), (std::vector{ trackRock.getId(), trackRockHappy.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, FilterExpr::Or({ isRock, isHappy }) })), (std::vector{ trackRock.getId(), trackRockHappy.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isHappy, FilterExpr::Or({ isHappy, isRock }) })), (std::vector{ trackRockHappy.getId(), trackHappy.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, FilterExpr::Not(isRock) })), std::vector<TrackId>{});
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, isHappy, FilterExpr::Or({ isRock, FilterExpr::Not(isHappy) }) })), (std::vector{ trackRockHappy.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_sameRelationsUnderOr)
    {
        ScopedRelease releaseRockJazz{ session, "RockJazz" };
        ScopedRelease releaseJazz{ session, "Jazz" };
        ScopedRelease releaseMetal{ session, "Metal" };
        ScopedRelease releaseNone{ session, "None" };
        ScopedTrack trackRock{ session };
        ScopedTrack trackRockJazz{ session };
        ScopedTrack trackJazz{ session };
        ScopedTrack trackMetal{ session };
        ScopedTrack trackNone{ session };
        ScopedArtist artistRock{ session, "ArtistRock" };
        ScopedArtist artistJazz{ session, "ArtistJazz" };
        ScopedArtist artistMetal{ session, "ArtistMetal" };
        ScopedGenre rock{ session, "Rock" };
        ScopedGenre jazz{ session, "Jazz" };
        ScopedGenre metal{ session, "Metal" };
        ScopedMood happy{ session, "Happy" };

        {
            auto transaction{ session.createWriteTransaction() };
            trackRock.get().modify()->setGenres(std::array{ rock.get() });
            trackRock.get().modify()->setRelease(releaseRockJazz.get());
            trackRockJazz.get().modify()->setGenres(std::array{ rock.get(), jazz.get() });
            trackRockJazz.get().modify()->setRelease(releaseRockJazz.get());
            trackJazz.get().modify()->setGenres(std::array{ jazz.get() });
            trackJazz.get().modify()->setMoods(std::array{ happy.get() });
            trackJazz.get().modify()->setRelease(releaseJazz.get());
            trackMetal.get().modify()->setGenres(std::array{ metal.get() });
            trackMetal.get().modify()->setRelease(releaseMetal.get());
            trackNone.get().modify()->setRelease(releaseNone.get());
            TrackArtistLink::create(session, trackRock.get(), artistRock.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, trackRockJazz.get(), artistRock.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, trackJazz.get(), artistJazz.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, trackMetal.get(), artistMetal.get(), TrackArtistLinkType::Artist);
        }

        const FilterExpr isRock{ FilterExpr::equal(rock.getId()) };
        const FilterExpr isJazz{ FilterExpr::equal(jazz.getId()) };
        const FilterExpr isMetal{ FilterExpr::equal(metal.getId()) };
        const FilterExpr isHappy{ FilterExpr::equal(happy.getId()) };

        // a track of both genres is returned once
        EXPECT_EQ(findTrackIds(session, FilterExpr::Or({ isRock, isJazz })), (std::vector{ trackRock.getId(), trackRockJazz.getId(), trackJazz.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Or({ isRock, isJazz, isMetal })), (std::vector{ trackRock.getId(), trackRockJazz.getId(), trackJazz.getId(), trackMetal.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Or({ isRock, isRock })), (std::vector{ trackRock.getId(), trackRockJazz.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(FilterExpr::Or({ isRock, isJazz }))), (std::vector{ trackMetal.getId(), trackNone.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Or({ isRock, isHappy, isMetal })), (std::vector{ trackRock.getId(), trackRockJazz.getId(), trackJazz.getId(), trackMetal.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isHappy, FilterExpr::Or({ isRock, isJazz, isMetal }) })), (std::vector{ trackJazz.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isJazz, FilterExpr::Or({ isJazz, isRock, isMetal }) })), (std::vector{ trackRockJazz.getId(), trackJazz.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, FilterExpr::Not(FilterExpr::Or({ isJazz, isMetal })) })), (std::vector{ trackRock.getId() }));
        // merged around a joined conjunct
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, FilterExpr::Or({ isJazz, isRock, isMetal, isHappy }) })), (std::vector{ trackRock.getId(), trackRockJazz.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isJazz, FilterExpr::Or({ isMetal, isRock, isHappy, isJazz }) })), (std::vector{ trackRockJazz.getId(), trackJazz.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ isRock, FilterExpr::Not(FilterExpr::Or({ isJazz, isRock, isMetal })) })), std::vector<TrackId>{});
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(FilterExpr::And({ isRock, FilterExpr::Or({ isJazz, isMetal }) }))), (std::vector{ trackRock.getId(), trackJazz.getId(), trackMetal.getId(), trackNone.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ FilterExpr::Not(FilterExpr::Or({ isJazz, isMetal })), FilterExpr::Or({ isRock, isHappy }) })), (std::vector{ trackRock.getId() }));

        // a release matches if one of its tracks does
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Or({ isRock, isJazz })), (std::vector{ releaseRockJazz.getId(), releaseJazz.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Not(FilterExpr::Or({ isRock, isJazz }))), (std::vector{ releaseMetal.getId(), releaseNone.getId() }));

        EXPECT_EQ(findArtistIds(session, FilterExpr::Or({ isRock, isJazz })), (std::vector{ artistRock.getId(), artistJazz.getId() }));
        EXPECT_EQ(findArtistIds(session, FilterExpr::Or({ isJazz, isMetal })), (std::vector{ artistRock.getId(), artistJazz.getId(), artistMetal.getId() }));
        EXPECT_EQ(findArtistIds(session, FilterExpr::Not(FilterExpr::Or({ isRock, isJazz }))), (std::vector{ artistMetal.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_releaseRelationsOfArtistsWithoutRelease)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack trackWithLabel{ session };
        ScopedTrack trackWithoutRelease{ session };
        ScopedArtist artistWithLabel{ session, "ArtistWithLabel" };
        ScopedArtist artistWithoutRelease{ session, "ArtistWithoutRelease" };
        ScopedLabel label{ session, "MyLabel" };
        ScopedLabel otherLabel{ session, "OtherLabel" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addLabel(label.get());
            trackWithLabel.get().modify()->setRelease(release.get());
            TrackArtistLink::create(session, trackWithLabel.get(), artistWithLabel.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, trackWithoutRelease.get(), artistWithoutRelease.get(), TrackArtistLinkType::Artist);
        }

        const FilterExpr hasLabel{ FilterExpr::equal(label.getId()) };
        const FilterExpr hasOtherLabel{ FilterExpr::equal(otherLabel.getId()) };

        EXPECT_EQ(findArtistIds(session, FilterExpr::Or({ hasLabel, hasOtherLabel })), (std::vector{ artistWithLabel.getId() }));
        // the track without release is not linked to any label
        EXPECT_EQ(findArtistIds(session, FilterExpr::Not(hasLabel)), (std::vector{ artistWithoutRelease.getId() }));
        EXPECT_EQ(findArtistIds(session, FilterExpr::Not(FilterExpr::Or({ hasLabel, hasOtherLabel }))), (std::vector{ artistWithoutRelease.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_trackAttributesAndUserData)
    {
        ScopedRelease compilation{ session, "MyCompilation" };
        ScopedTrack shortTrack{ session };
        ScopedTrack longTrack{ session };
        ScopedTrack unknownPropertiesTrack{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRatedTrack rating{ session, longTrack.lockAndGet(), user.lockAndGet() };
        ScopedTrackFeedback feedback{ session, shortTrack.lockAndGet(), user.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            compilation.get().modify()->setCompilation(true);
            shortTrack.get().modify()->setDuration(std::chrono::seconds{ 30 });
            shortTrack.get().modify()->setBitrate(128000);
            shortTrack.get().modify()->setCodec(core::media::Codec::MP3);
            shortTrack.get().modify()->setRelease(compilation.get());
            longTrack.get().modify()->setDuration(std::chrono::minutes{ 10 });
            rating.get().modify()->setRating(4);
            feedback.get().modify()->setValue(FeedbackValue::Loved);
        }

        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(DurationCondition{ FilterOperator::Greater, std::chrono::milliseconds{ std::chrono::minutes{ 1 } } })), (std::vector{ longTrack.getId() }));
        // unknown values are stored as 0 and must behave as missing
        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(DurationCondition{ FilterOperator::Less, std::chrono::milliseconds{ std::chrono::minutes{ 1 } } })), (std::vector{ shortTrack.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(FilterExpr::condition(DurationCondition{ FilterOperator::Greater, std::chrono::milliseconds{ std::chrono::minutes{ 1 } } }))), (std::vector{ shortTrack.getId(), unknownPropertiesTrack.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(IntegerCondition{ IntegerField::Bitrate, FilterOperator::LessOrEqual, std::int64_t{ 128000 } })), (std::vector{ shortTrack.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::equal(core::media::Codec::MP3)), (std::vector{ shortTrack.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(IsCompilationCondition{ true })), (std::vector{ shortTrack.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(RatingCondition{ FilterOperator::GreaterOrEqual, std::int64_t{ 3 } }), user.getId()), (std::vector{ longTrack.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::condition(FeedbackCondition{ FeedbackValue::Loved }), user.getId()), (std::vector{ shortTrack.getId() }));

        EXPECT_EQ(findReleaseIds(session, FilterExpr::condition(IsCompilationCondition{ true })), (std::vector{ compilation.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::condition(DurationCondition{ FilterOperator::Equal, std::chrono::milliseconds{ std::chrono::seconds{ 30 } } })), (std::vector{ compilation.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_trackClusters)
    {
        ScopedTrack trackBoth{ session };
        ScopedTrack trackFirstOnly{ session };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

        {
            auto transaction{ session.createWriteTransaction() };
            cluster1.get().modify()->addTrack(trackBoth.get());
            cluster2.get().modify()->addTrack(trackBoth.get());
            cluster1.get().modify()->addTrack(trackFirstOnly.get());
        }

        const FilterExpr expr{ FilterExpr::And({ FilterExpr::equal(cluster1.getId()),
                                                 FilterExpr::equal(cluster2.getId()) }) };
        EXPECT_EQ(findTrackIds(session, expr), (std::vector{ trackBoth.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_releaseYearAndListens)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track1985{ session };
        ScopedTrack track1995{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createWriteTransaction() };
            track1985.get().modify()->setDate(core::PartialDateTime{ 1985 });
            track1985.get().modify()->setRelease(release.get());
            track1995.get().modify()->setDate(core::PartialDateTime{ 1995 });
            track1995.get().modify()->setRelease(release.get());
        }

        const std::vector expected{ release.getId() };

        EXPECT_EQ(findReleaseIds(session, year(FilterOperator::Greater, 1990)), expected);
        EXPECT_EQ(findReleaseIds(session, year(FilterOperator::Less, 1990)), expected);
        // both bounds must be satisfied by the same track
        EXPECT_TRUE(findReleaseIds(session, FilterExpr::condition(IntegerCondition{ IntegerField::Year, FilterOperator::Between, FilterRange<std::int64_t>{ 1988, 1992 } })).empty());

        EXPECT_EQ(findReleaseIds(session, listenCount(FilterOperator::Equal, 0), user.getId()), expected);

        ScopedListen listen1{ session, user.lockAndGet(), track1985.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2020, 1, 1 } } };
        ScopedListen listen2{ session, user.lockAndGet(), track1995.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2020, 1, 2 } } };

        // conditions apply to a single track
        EXPECT_EQ(findReleaseIds(session, listenCount(FilterOperator::Equal, 1), user.getId()), expected);
        EXPECT_TRUE(findReleaseIds(session, listenCount(FilterOperator::Equal, 2), user.getId()).empty());
        EXPECT_TRUE(findReleaseIds(session, listenCount(FilterOperator::Equal, 0), user.getId()).empty());

        {
            // the filter combines with a sort joining the tracks
            auto transaction{ session.createReadTransaction() };
            const auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(year(FilterOperator::Greater, 1990)).setSortMethod(ReleaseSortMethod::DateAsc)) };
            EXPECT_EQ(releases, expected);
        }
    }

    TEST_F(DatabaseFixture, FilterExpr_releaseSingleTrackSemantics)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack trackRock{ session };
        ScopedTrack trackHappy{ session };
        ScopedGenre rock{ session, "Rock" };
        ScopedMood happy{ session, "Happy" };

        {
            auto transaction{ session.createWriteTransaction() };
            trackRock.get().modify()->setGenres(std::array{ rock.get() });
            trackRock.get().modify()->setRelease(release.get());
            trackHappy.get().modify()->setMoods(std::array{ happy.get() });
            trackHappy.get().modify()->setRelease(release.get());
        }

        const FilterExpr isRock{ FilterExpr::equal(rock.getId()) };
        const FilterExpr isHappy{ FilterExpr::equal(happy.getId()) };
        const std::vector expected{ release.getId() };

        EXPECT_EQ(findReleaseIds(session, isRock), expected);
        EXPECT_TRUE(findReleaseIds(session, FilterExpr::And({ isRock, isHappy })).empty());
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Or({ isRock, isHappy })), expected);
        // the happy track is not rock
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Not(isRock)), expected);
    }

    TEST_F(DatabaseFixture, FilterExpr_releaseConditionsWithoutTracks)
    {
        ScopedRelease releaseWithoutTracks{ session, "ReleaseWithoutTracks" };
        ScopedRelease releaseWithTrack{ session, "ReleaseWithTrack" };
        ScopedTrack track{ session };
        ScopedLabel label{ session, "MyLabel" };
        ScopedCountry country{ session, "MyCountry" };
        ScopedGenre rock{ session, "Rock" };

        {
            auto transaction{ session.createWriteTransaction() };
            releaseWithoutTracks.get().modify()->addLabel(label.get());
            releaseWithoutTracks.get().modify()->addCountry(country.get());
            releaseWithoutTracks.get().modify()->setCompilation(true);
            track.get().modify()->setRelease(releaseWithTrack.get());
            track.get().modify()->setGenres(std::array{ rock.get() });
        }

        const FilterExpr hasLabel{ FilterExpr::equal(label.getId()) };

        // a filter selects tracks: a release without tracks never matches, even on its own conditions
        EXPECT_TRUE(findReleaseIds(session, hasLabel).empty());
        EXPECT_TRUE(findReleaseIds(session, FilterExpr::equal(country.getId())).empty());
        EXPECT_TRUE(findReleaseIds(session, FilterExpr::condition(IsCompilationCondition{ true })).empty());
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Not(hasLabel)), (std::vector{ releaseWithTrack.getId() }));
        EXPECT_TRUE(findReleaseIds(session, FilterExpr::And({ hasLabel, FilterExpr::equal(rock.getId()) })).empty());
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Or({ hasLabel, FilterExpr::equal(rock.getId()) })), (std::vector{ releaseWithTrack.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_releaseConditionsOnTracks)
    {
        ScopedRelease releaseWithLabel{ session, "ReleaseWithLabel" };
        ScopedRelease releaseWithoutLabel{ session, "ReleaseWithoutLabel" };
        ScopedTrack trackWithLabel{ session };
        ScopedTrack trackWithoutLabel{ session };
        ScopedTrack trackWithoutRelease{ session };
        ScopedLabel label{ session, "MyLabel" };
        ScopedLabel otherLabel{ session, "OtherLabel" };

        {
            auto transaction{ session.createWriteTransaction() };
            releaseWithLabel.get().modify()->addLabel(label.get());
            releaseWithLabel.get().modify()->setCompilation(true);
            trackWithLabel.get().modify()->setRelease(releaseWithLabel.get());
            trackWithoutLabel.get().modify()->setRelease(releaseWithoutLabel.get());
        }

        const FilterExpr hasLabel{ FilterExpr::equal(label.getId()) };
        const FilterExpr isCompilation{ FilterExpr::condition(IsCompilationCondition{ true }) };
        const std::vector others{ trackWithoutLabel.getId(), trackWithoutRelease.getId() };

        EXPECT_EQ(findTrackIds(session, hasLabel), (std::vector{ trackWithLabel.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(hasLabel)), others);
        EXPECT_EQ(findTrackIds(session, FilterExpr::Or({ hasLabel, FilterExpr::equal(otherLabel.getId()) })), (std::vector{ trackWithLabel.getId() }));
        // tracks without release are not linked to any label
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(FilterExpr::Or({ hasLabel, FilterExpr::equal(otherLabel.getId()) }))), others);
        EXPECT_EQ(findTrackIds(session, isCompilation), (std::vector{ trackWithLabel.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(isCompilation)), others);
    }

    TEST_F(DatabaseFixture, FilterExpr_releaseConditionsMixedWithTrackConditions)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedRelease otherRelease{ session, "OtherRelease" };
        ScopedTrack trackRock{ session };
        ScopedTrack trackPop{ session };
        ScopedTrack otherTrackRock{ session };
        ScopedArtist rockArtist{ session, "RockArtist" };
        ScopedArtist popArtist{ session, "PopArtist" };
        ScopedArtist otherArtist{ session, "OtherArtist" };
        ScopedLabel label{ session, "MyLabel" };
        ScopedGenre rock{ session, "Rock" };
        ScopedGenre pop{ session, "Pop" };
        ScopedGenre jazz{ session, "Jazz" };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addLabel(label.get());
            trackRock.get().modify()->setRelease(release.get());
            trackRock.get().modify()->setGenres(std::array{ rock.get() });
            trackPop.get().modify()->setRelease(release.get());
            trackPop.get().modify()->setGenres(std::array{ pop.get() });
            otherTrackRock.get().modify()->setRelease(otherRelease.get());
            otherTrackRock.get().modify()->setGenres(std::array{ rock.get() });
            TrackArtistLink::create(session, trackRock.get(), rockArtist.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, trackPop.get(), popArtist.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, otherTrackRock.get(), otherArtist.get(), TrackArtistLinkType::Artist);
        }

        const FilterExpr hasLabel{ FilterExpr::equal(label.getId()) };
        const FilterExpr isRock{ FilterExpr::equal(rock.getId()) };
        const FilterExpr isJazz{ FilterExpr::equal(jazz.getId()) };

        EXPECT_EQ(findReleaseIds(session, FilterExpr::And({ hasLabel, isRock })), (std::vector{ release.getId() }));
        EXPECT_TRUE(findReleaseIds(session, FilterExpr::And({ hasLabel, isJazz })).empty());
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Or({ hasLabel, isJazz })), (std::vector{ release.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Or({ isJazz, FilterExpr::Not(hasLabel) })), (std::vector{ otherRelease.getId() }));
        // the pop track of the release satisfies the expression
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Not(FilterExpr::And({ hasLabel, isRock }))), (std::vector{ release.getId(), otherRelease.getId() }));

        // artists match through their tracks
        EXPECT_EQ(findArtistIds(session, hasLabel), (std::vector{ rockArtist.getId(), popArtist.getId() }));
        EXPECT_EQ(findArtistIds(session, FilterExpr::And({ hasLabel, isRock })), (std::vector{ rockArtist.getId() }));
        EXPECT_EQ(findArtistIds(session, FilterExpr::Not(hasLabel)), (std::vector{ otherArtist.getId() }));

        ScopedListen listen1{ session, user.lockAndGet(), trackRock.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2020, 1, 1 } } };
        ScopedListen listen2{ session, user.lockAndGet(), otherTrackRock.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2020, 1, 2 } } };
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Listen::getTopReleases(session, Listen::StatsFindParameters{}.setUser(user.getId()).setFilter(hasLabel)), (std::vector{ release.getId() }));
        }
    }

    TEST_F(DatabaseFixture, FilterExpr_advisory)
    {
        ScopedRelease mixedRelease{ session, "MixedRelease" };
        ScopedRelease cleanRelease{ session, "CleanRelease" };
        ScopedTrack explicitTrack{ session };
        ScopedTrack cleanTrack1{ session };
        ScopedTrack cleanTrack2{ session };
        ScopedTrack unsetTrack{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            explicitTrack.get().modify()->setAdvisory(Advisory::Explicit);
            explicitTrack.get().modify()->setRelease(mixedRelease.get());
            cleanTrack1.get().modify()->setAdvisory(Advisory::Clean);
            cleanTrack1.get().modify()->setRelease(mixedRelease.get());
            cleanTrack2.get().modify()->setAdvisory(Advisory::Clean);
            cleanTrack2.get().modify()->setRelease(cleanRelease.get());
        }

        const FilterExpr isExplicit{ FilterExpr::equal(Advisory::Explicit) };

        EXPECT_EQ(findTrackIds(session, isExplicit), (std::vector{ explicitTrack.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::equal(Advisory::Clean)), (std::vector{ cleanTrack1.getId(), cleanTrack2.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(isExplicit)), (std::vector{ cleanTrack1.getId(), cleanTrack2.getId(), unsetTrack.getId() }));

        // releases match if one of their tracks matches
        EXPECT_EQ(findReleaseIds(session, isExplicit), (std::vector{ mixedRelease.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::equal(Advisory::Clean)), (std::vector{ mixedRelease.getId(), cleanRelease.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Not(isExplicit)), (std::vector{ mixedRelease.getId(), cleanRelease.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_artistTracks)
    {
        ScopedArtist releaseArtist{ session, "MyReleaseArtist" };
        ScopedArtist trackArtist{ session, "MyTrackArtist" };
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{ session };
        ScopedGenre rock{ session, "Rock" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setGenres(std::array{ rock.get() });
            track.get().modify()->setRelease(release.get());
            TrackArtistLink::create(session, track.get(), trackArtist.get(), TrackArtistLinkType::Artist);
            ReleaseArtistLink::create(session, release.get(), releaseArtist.get(), false);
        }

        auto findArtistIds{ [&](Artist::FindParameters params) {
            auto transaction{ session.createReadTransaction() };
            return Artist::findIds(session, params.setFilter(FilterExpr::equal(rock.getId())).setSortMethod(ArtistSortMethod::Id));
        } };

        EXPECT_EQ(findArtistIds(Artist::FindParameters{}), (std::vector{ releaseArtist.getId(), trackArtist.getId() }));
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setReleaseArtistsOnly(true)), (std::vector{ releaseArtist.getId() }));
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setTrackArtistLinkType(TrackArtistLinkType::Artist)), (std::vector{ trackArtist.getId() }));
        EXPECT_TRUE(findArtistIds(Artist::FindParameters{}.setTrackArtistLinkType(TrackArtistLinkType::Composer)).empty());
    }

    TEST_F(DatabaseFixture, FilterExpr_artistLinkTypeAndFilterOnSameTrack)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedArtist releaseArtist{ session, "MyReleaseArtist" };
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack trackInLibrary1{ session };
        ScopedTrack trackInLibrary2{ session };
        ScopedMediaLibrary library1{ session, "Library1", "/root1" };
        ScopedMediaLibrary library2{ session, "Library2", "/root2" };

        {
            auto transaction{ session.createWriteTransaction() };
            trackInLibrary1.get().modify()->setMediaLibrary(library1.get());
            trackInLibrary2.get().modify()->setMediaLibrary(library2.get());
            trackInLibrary2.get().modify()->setRelease(release.get());
            TrackArtistLink::create(session, trackInLibrary1.get(), artist.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, trackInLibrary2.get(), artist.get(), TrackArtistLinkType::Composer);
            ReleaseArtistLink::create(session, release.get(), releaseArtist.get(), false);
        }

        auto findArtistIds{ [&](MediaLibraryId library, TrackArtistLinkType linkType) {
            auto transaction{ session.createReadTransaction() };
            return Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(library)).setTrackArtistLinkType(linkType).setSortMethod(ArtistSortMethod::Id));
        } };

        // the link type and the filter must hold on the same track
        EXPECT_EQ(findArtistIds(library1.getId(), TrackArtistLinkType::Artist), (std::vector{ artist.getId() }));
        EXPECT_EQ(findArtistIds(library2.getId(), TrackArtistLinkType::Composer), (std::vector{ artist.getId() }));
        EXPECT_TRUE(findArtistIds(library1.getId(), TrackArtistLinkType::Composer).empty());
        EXPECT_TRUE(findArtistIds(library2.getId(), TrackArtistLinkType::Artist).empty());
        // releaseArtist is never returned when a link type is set: it has no linked track
    }

    TEST_F(DatabaseFixture, FilterExpr_artistTracksSort)
    {
        ScopedArtist artistA{ session, "ArtistA" };
        ScopedArtist artistC{ session, "ArtistC" };
        ScopedArtist artistD{ session, "ArtistD" };
        ScopedArtist artistV{ session, "ArtistV" };
        ScopedRelease releaseB{ session, "ReleaseB" };
        ScopedRelease releaseV{ session, "ReleaseV" };
        ScopedTrack trackA{ session };
        ScopedTrack trackB{ session };
        ScopedTrack trackV{ session };
        ScopedGenre rock{ session, "Rock" };
        ScopedUser user{ session, "MyUser" };

        const Wt::WDateTime date2020{ Wt::WDate{ 2020, 1, 1 } };
        const Wt::WDateTime date2021{ Wt::WDate{ 2021, 1, 1 } };
        const Wt::WDateTime date2022{ Wt::WDate{ 2022, 1, 1 } };

        {
            auto transaction{ session.createWriteTransaction() };

            auto setUpTrack{ [&](Track::pointer track, const Wt::WDateTime& dateTime) {
                track.modify()->setGenres(std::array{ rock.get() });
                track.modify()->setAddedTime(dateTime);
                track.modify()->setLastWriteTime(dateTime);
            } };

            // artistA: linked to a 2020 track, and release artist of a 2022 release
            setUpTrack(trackA.get(), date2020);
            TrackArtistLink::create(session, trackA.get(), artistA.get(), TrackArtistLinkType::Artist);

            setUpTrack(trackB.get(), date2022);
            trackB.get().modify()->setRelease(releaseB.get());
            TrackArtistLink::create(session, trackB.get(), artistC.get(), TrackArtistLinkType::Artist);
            ReleaseArtistLink::create(session, releaseB.get(), artistA.get(), false);

            // artistV: release artist only
            setUpTrack(trackV.get(), date2021);
            trackV.get().modify()->setRelease(releaseV.get());
            TrackArtistLink::create(session, trackV.get(), artistD.get(), TrackArtistLinkType::Artist);
            ReleaseArtistLink::create(session, releaseV.get(), artistV.get(), false);

            auto star{ [&](Artist::pointer artist, const Wt::WDateTime& dateTime) {
                auto feedback{ ArtistFeedback::create(session, artist, user.get()) };
                feedback.modify()->setValue(FeedbackValue::Loved);
                feedback.modify()->setDateTime(dateTime);
            } };
            star(artistA.get(), date2021);
            star(artistC.get(), date2020);
            star(artistV.get(), date2022);
        }

        auto findArtistIds{ [&](Artist::FindParameters params) {
            auto transaction{ session.createReadTransaction() };
            return Artist::findIds(session, params.setFilter(FilterExpr::equal(rock.getId())));
        } };

        // oldest track across both kinds of artist tracks
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setSortMethod(ArtistSortMethod::AddedDesc)), (std::vector{ artistC.getId(), artistD.getId(), artistV.getId(), artistA.getId() }));
        // most recent track across both kinds of artist tracks
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setSortMethod(ArtistSortMethod::LastWrittenDesc)), (std::vector{ artistA.getId(), artistC.getId(), artistD.getId(), artistV.getId() }));
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setSortMethod(ArtistSortMethod::FeedbackDateDesc).setFeedbackUser(user.getId()).setFeedbackValue(FeedbackValue::Loved)), (std::vector{ artistV.getId(), artistA.getId(), artistC.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_artistTracksSortOnlyConsidersTheMatchingTracks)
    {
        ScopedArtist artistX{ session, "ArtistX" };
        ScopedArtist artistY{ session, "ArtistY" };
        ScopedTrack trackX1{ session };
        ScopedTrack trackX2{ session };
        ScopedTrack trackY{ session };
        ScopedMediaLibrary library1{ session, "Library1", "/root1" };
        ScopedMediaLibrary library2{ session, "Library2", "/root2" };

        {
            auto transaction{ session.createWriteTransaction() };

            auto setUpTrack{ [&](Track::pointer track, const MediaLibrary::pointer& library, const Artist::pointer& artist, int addedYear, int writtenYear) {
                track.modify()->setMediaLibrary(library);
                track.modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ addedYear, 1, 1 } });
                track.modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ writtenYear, 1, 1 } });
                TrackArtistLink::create(session, track, artist, TrackArtistLinkType::Artist);
            } };

            // artistX was added to library1 after artistY, but long before to library2, where it was also modified later
            setUpTrack(trackX1.get(), library1.get(), artistX.get(), 2022, 2020);
            setUpTrack(trackX2.get(), library2.get(), artistX.get(), 2020, 2022);
            setUpTrack(trackY.get(), library1.get(), artistY.get(), 2021, 2021);
        }

        auto findArtistIds{ [&](Artist::FindParameters params) {
            auto transaction{ session.createReadTransaction() };
            return Artist::findIds(session, params.setFilter(FilterExpr::equal(library1.getId())));
        } };

        // the tracks of library2 do not count, with both kinds of artist tracks or with a link type
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setSortMethod(ArtistSortMethod::AddedDesc)), (std::vector{ artistX.getId(), artistY.getId() }));
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setSortMethod(ArtistSortMethod::LastWrittenDesc)), (std::vector{ artistY.getId(), artistX.getId() }));
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setSortMethod(ArtistSortMethod::AddedDesc).setTrackArtistLinkType(TrackArtistLinkType::Artist)), (std::vector{ artistX.getId(), artistY.getId() }));
        EXPECT_EQ(findArtistIds(Artist::FindParameters{}.setSortMethod(ArtistSortMethod::LastWrittenDesc).setTrackArtistLinkType(TrackArtistLinkType::Artist)), (std::vector{ artistY.getId(), artistX.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_artistWithKeywords)
    {
        ScopedArtist artist{ session, "MyArtist" };
        ScopedArtist otherArtist{ session, "MyOtherArtist" };
        ScopedTrack track{ session };
        ScopedTrack otherTrack{ session };
        ScopedGenre rock{ session, "Rock" };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setGenres(std::array{ rock.get() });
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Composer);
            TrackArtistLink::create(session, otherTrack.get(), otherArtist.get(), TrackArtistLinkType::Artist);
        }

        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), Wt::WDateTime{ Wt::WDate{ 2020, 1, 1 } } };

        const FilterExpr isRock{ FilterExpr::equal(rock.getId()) };

        auto findArtistIds{ [&](const FilterExpr& expr) {
            auto transaction{ session.createReadTransaction() };
            return Artist::findIds(session, Artist::FindParameters{}.setKeywords({ "My" }).setFilter(expr, user.getId()).setSortMethod(ArtistSortMethod::Id));
        } };

        EXPECT_EQ(findArtistIds(isRock), (std::vector{ artist.getId() }));
        EXPECT_EQ(findArtistIds(FilterExpr::Not(isRock)), (std::vector{ otherArtist.getId() }));
        // several links to the same track must not count the same listen twice
        EXPECT_EQ(findArtistIds(listenCount(FilterOperator::Equal, 1)), (std::vector{ artist.getId() }));
        EXPECT_EQ(findArtistIds(FilterExpr::And({ isRock, listenCount(FilterOperator::Greater, 0) })), (std::vector{ artist.getId() }));
    }
} // namespace lms::db::tests
