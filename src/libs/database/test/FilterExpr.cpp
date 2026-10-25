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

        std::vector<TrackId> findTrackIds(Session& session, const FilterExpr& expr, UserId user = {})
        {
            auto transaction{ session.createReadTransaction() };
            return Track::findIds(session, Track::FindParameters{}.setFilter(expr, user).setSortMethod(TrackSortMethod::Id));
        }

        std::vector<ReleaseId> findReleaseIds(Session& session, const FilterExpr& expr, UserId user = {})
        {
            auto transaction{ session.createReadTransaction() };
            return Release::findIds(session, Release::FindParameters{}.setFilter(expr, user).setSortMethod(ReleaseSortMethod::Id));
        }

        std::vector<ArtistId> findArtistIds(Session& session, const FilterExpr& expr, UserId user = {})
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

        FilterExpr name(StringField field, StringOperator op, std::string value)
        {
            return FilterExpr::condition(StringCondition{ field, op, std::move(value) });
        }

        void createCredit(Session& session, const Track::pointer& track, const Artist::pointer& artist, std::string_view creditedName, TrackArtistLinkType type = TrackArtistLinkType::Artist, std::string_view creditedSortName = {})
        {
            auto link{ session.create<TrackArtistLink>(track, artist, type) };
            link.modify()->setArtistName(creditedName);
            link.modify()->setArtistSortName(creditedSortName);
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
            session.create<TrackArtistLink>(trackRock.get(), artistRock.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackRockJazz.get(), artistRock.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackJazz.get(), artistJazz.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackMetal.get(), artistMetal.get(), TrackArtistLinkType::Artist);
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
            session.create<TrackArtistLink>(trackWithLabel.get(), artistWithLabel.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackWithoutRelease.get(), artistWithoutRelease.get(), TrackArtistLinkType::Artist);
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
            // the track join is shared between the filter and the sort
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

        // release conditions alone apply to the release itself, its tracks are not needed
        EXPECT_EQ(findReleaseIds(session, hasLabel), (std::vector{ releaseWithoutTracks.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::equal(country.getId())), (std::vector{ releaseWithoutTracks.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::condition(IsCompilationCondition{ true })), (std::vector{ releaseWithoutTracks.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Not(hasLabel)), (std::vector{ releaseWithTrack.getId() }));

        // track conditions need a track
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
            session.create<TrackArtistLink>(trackRock.get(), rockArtist.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackPop.get(), popArtist.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(otherTrackRock.get(), otherArtist.get(), TrackArtistLinkType::Artist);
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

    TEST_F(DatabaseFixture, FilterExpr_trackNameFilter)
    {
        ScopedTrack helloWorld{ session };
        ScopedTrack helloThere{ session };
        ScopedTrack goodbye{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            helloWorld.get().modify()->setName("Hello World");
            helloThere.get().modify()->setName("hello there");
            goodbye.get().modify()->setName("Goodbye");
        }

        EXPECT_EQ(findTrackIds(session, name(StringField::TrackName, StringOperator::Equal, "hello world")), (std::vector{ helloWorld.getId() }));
        EXPECT_TRUE(findTrackIds(session, name(StringField::TrackName, StringOperator::Equal, "hello")).empty());
        EXPECT_EQ(findTrackIds(session, name(StringField::TrackName, StringOperator::StartsWith, "HELLO")), (std::vector{ helloWorld.getId(), helloThere.getId() }));
        EXPECT_EQ(findTrackIds(session, name(StringField::TrackName, StringOperator::Contains, "o w")), (std::vector{ helloWorld.getId() }));
        EXPECT_EQ(findTrackIds(session, name(StringField::TrackName, StringOperator::EndsWith, "bye")), (std::vector{ goodbye.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(name(StringField::TrackName, StringOperator::StartsWith, "hello"))), (std::vector{ goodbye.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_releaseNameFilter)
    {
        ScopedRelease abbeyRoad{ session, "Abbey Road" };
        ScopedRelease roadTrip{ session, "Road Trip" };
        ScopedRelease abbeyEmpty{ session, "Abbey Empty" };
        ScopedTrack abbeyRoadTrack1{ session };
        ScopedTrack abbeyRoadTrack2{ session };
        ScopedTrack roadTripTrack{ session };
        ScopedArtist abbeyRoadArtist{ session, "AbbeyRoadArtist" };
        ScopedArtist roadTripArtist{ session, "RoadTripArtist" };
        ScopedGenre rock{ session, "Rock" };

        {
            auto transaction{ session.createWriteTransaction() };
            abbeyRoadTrack1.get().modify()->setRelease(abbeyRoad.get());
            abbeyRoadTrack2.get().modify()->setRelease(abbeyRoad.get());
            abbeyRoadTrack2.get().modify()->setGenres(std::array{ rock.get() });
            roadTripTrack.get().modify()->setRelease(roadTrip.get());
            roadTripTrack.get().modify()->setGenres(std::array{ rock.get() });
            session.create<TrackArtistLink>(abbeyRoadTrack1.get(), abbeyRoadArtist.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(roadTripTrack.get(), roadTripArtist.get(), TrackArtistLinkType::Artist);
        }

        const FilterExpr startsWithAbbey{ name(StringField::ReleaseName, StringOperator::StartsWith, "abbey") };

        // the release without tracks matches: no track join
        EXPECT_EQ(findReleaseIds(session, startsWithAbbey), (std::vector{ abbeyRoad.getId(), abbeyEmpty.getId() }));
        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseName, StringOperator::EndsWith, "ROAD")), (std::vector{ abbeyRoad.getId() }));
        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseName, StringOperator::Contains, "road")), (std::vector{ abbeyRoad.getId(), roadTrip.getId() }));
        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseName, StringOperator::Equal, "road trip")), (std::vector{ roadTrip.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::Not(startsWithAbbey)), (std::vector{ roadTrip.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::And({ startsWithAbbey, FilterExpr::equal(rock.getId()) })), (std::vector{ abbeyRoad.getId() }));

        // tracks and artists match through the release of their tracks
        EXPECT_EQ(findTrackIds(session, startsWithAbbey), (std::vector{ abbeyRoadTrack1.getId(), abbeyRoadTrack2.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ startsWithAbbey, FilterExpr::equal(rock.getId()) })), (std::vector{ abbeyRoadTrack2.getId() }));
        EXPECT_EQ(findArtistIds(session, startsWithAbbey), (std::vector{ abbeyRoadArtist.getId() }));
        EXPECT_EQ(findArtistIds(session, FilterExpr::Not(startsWithAbbey)), (std::vector{ roadTripArtist.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_releaseSortNameFilter)
    {
        ScopedRelease theWall{ session, "The Wall" };
        ScopedRelease animals{ session, "Animals" };

        {
            auto transaction{ session.createWriteTransaction() };
            theWall.get().modify()->setSortName("Wall, The");
            animals.get().modify()->setSortName("");
        }

        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseSortName, StringOperator::StartsWith, "wall")), (std::vector{ theWall.getId() }));
        EXPECT_TRUE(findReleaseIds(session, name(StringField::ReleaseSortName, StringOperator::StartsWith, "the")).empty());
        // falls back on the name
        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseSortName, StringOperator::StartsWith, "anim")), (std::vector{ animals.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_artistNameFilterCreditedAndOwnName)
    {
        ScopedArtist rhapsody{ session, "Rhapsody of Fire" };
        ScopedArtist luca{ session, "Luca Turilli" };
        ScopedArtist justice{ session, "Justice" };
        ScopedArtist withoutCredit{ session, "Rhapsody Without Credit" };
        ScopedRelease oldRelease{ session, "Legendary Tales" };
        ScopedRelease newRelease{ session, "Dark Wings" };
        ScopedRelease justiceRelease{ session, "Cross" };
        ScopedTrack creditedOld{ session };
        ScopedTrack creditedNew{ session };
        ScopedTrack justiceTrack{ session };
        ScopedGenre rock{ session, "Rock" };

        {
            auto transaction{ session.createWriteTransaction() };
            creditedOld.get().modify()->setRelease(oldRelease.get());
            creditedOld.get().modify()->setGenres(std::array{ rock.get() });
            creditedNew.get().modify()->setRelease(newRelease.get());
            justiceTrack.get().modify()->setRelease(justiceRelease.get());
            justiceTrack.get().modify()->setGenres(std::array{ rock.get() });
            // same artist, credited under its former name
            createCredit(session, creditedOld.get(), rhapsody.get(), "Rhapsody");
            createCredit(session, creditedNew.get(), rhapsody.get(), "Rhapsody of Fire");
            createCredit(session, creditedNew.get(), luca.get(), "Luca Turilli", TrackArtistLinkType::Composer);
            createCredit(session, justiceTrack.get(), justice.get(), "Justice");
        }

        const FilterExpr formerName{ name(StringField::ArtistName, StringOperator::Equal, "rhapsody") };
        const FilterExpr startsWithRhapsody{ name(StringField::ArtistName, StringOperator::StartsWith, "RHAPSODY") };
        const FilterExpr currentName{ name(StringField::ArtistName, StringOperator::Equal, "Rhapsody of Fire") };
        const FilterExpr isRock{ FilterExpr::equal(rock.getId()) };

        // tracks: matches the credited name or the name of the artist, whatever the link type
        EXPECT_EQ(findTrackIds(session, formerName), (std::vector{ creditedOld.getId() }));
        EXPECT_EQ(findTrackIds(session, currentName), (std::vector{ creditedOld.getId(), creditedNew.getId() }));
        EXPECT_EQ(findTrackIds(session, startsWithRhapsody), (std::vector{ creditedOld.getId(), creditedNew.getId() }));
        EXPECT_EQ(findTrackIds(session, name(StringField::ArtistName, StringOperator::StartsWith, "luca")), (std::vector{ creditedNew.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Not(formerName)), (std::vector{ creditedNew.getId(), justiceTrack.getId() }));
        // on the same track
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ startsWithRhapsody, isRock })), (std::vector{ creditedOld.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::And({ startsWithRhapsody, FilterExpr::Not(isRock) })), (std::vector{ creditedNew.getId() }));
        EXPECT_EQ(findTrackIds(session, FilterExpr::Or({ formerName, name(StringField::ArtistName, StringOperator::Equal, "justice") })), (std::vector{ creditedOld.getId(), justiceTrack.getId() }));

        // releases: one of their tracks matches
        EXPECT_EQ(findReleaseIds(session, formerName), (std::vector{ oldRelease.getId() }));
        EXPECT_EQ(findReleaseIds(session, startsWithRhapsody), (std::vector{ oldRelease.getId(), newRelease.getId() }));
        EXPECT_EQ(findReleaseIds(session, FilterExpr::And({ startsWithRhapsody, isRock })), (std::vector{ oldRelease.getId() }));

        // artists: only their own credits count, and an artist without credit does not match
        EXPECT_EQ(findArtistIds(session, formerName), (std::vector{ rhapsody.getId() }));
        EXPECT_EQ(findArtistIds(session, startsWithRhapsody), (std::vector{ rhapsody.getId() }));
        EXPECT_EQ(findArtistIds(session, name(StringField::ArtistName, StringOperator::StartsWith, "luca")), (std::vector{ luca.getId() }));
        EXPECT_EQ(findArtistIds(session, name(StringField::ArtistName, StringOperator::Equal, "justice")), (std::vector{ justice.getId() }));
        EXPECT_EQ(findArtistIds(session, FilterExpr::And({ startsWithRhapsody, isRock })), (std::vector{ rhapsody.getId() }));
        EXPECT_EQ(findArtistIds(session, FilterExpr::Not(startsWithRhapsody)), (std::vector{ luca.getId(), justice.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_artistNameFilterReleaseCredit)
    {
        ScopedArtist variousArtists{ session, "Various Artists" };
        ScopedArtist daftPunk{ session, "Daft Punk" };
        ScopedArtist justice{ session, "Justice" };
        ScopedRelease hits{ session, "Hits" };
        ScopedRelease other{ session, "Other" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack otherTrack{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setRelease(hits.get());
            track2.get().modify()->setRelease(hits.get());
            otherTrack.get().modify()->setRelease(other.get());
            createCredit(session, track1.get(), daftPunk.get(), "Daft Punk");
            createCredit(session, track2.get(), justice.get(), "Justice");
            createCredit(session, otherTrack.get(), justice.get(), "Justice");
            auto releaseCredit{ session.create<ReleaseArtistLink>(hits.get(), variousArtists.get(), false) };
            releaseCredit.modify()->setArtistName("Various Artists");
        }

        const FilterExpr various{ name(StringField::ArtistName, StringOperator::Equal, "various artists") };

        // the release credit applies to all the tracks of the release
        EXPECT_EQ(findTrackIds(session, various), (std::vector{ track1.getId(), track2.getId() }));
        EXPECT_EQ(findTrackIds(session, name(StringField::ArtistName, StringOperator::Equal, "daft punk")), (std::vector{ track1.getId() }));
        EXPECT_EQ(findReleaseIds(session, various), (std::vector{ hits.getId() }));
        EXPECT_EQ(findReleaseIds(session, name(StringField::ArtistName, StringOperator::Equal, "justice")), (std::vector{ hits.getId(), other.getId() }));
        EXPECT_EQ(findArtistIds(session, various), (std::vector{ variousArtists.getId() }));
        EXPECT_EQ(findArtistIds(session, name(StringField::ArtistName, StringOperator::Equal, "daft punk")), (std::vector{ daftPunk.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_artistSortNameFilter)
    {
        ScopedArtist beatles{ session, "The Beatles" };
        ScopedArtist animals{ session, "Animals" };
        ScopedTrack beatlesTrack{ session };
        ScopedTrack animalsTrack{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            beatles.get().modify()->setSortName("Beatles, The");
            animals.get().modify()->setSortName("");
            createCredit(session, beatlesTrack.get(), beatles.get(), "The Beatles", TrackArtistLinkType::Artist, "Beatles, The");
            createCredit(session, animalsTrack.get(), animals.get(), "Animals");
        }

        const FilterExpr startsWithBeatles{ name(StringField::ArtistSortName, StringOperator::StartsWith, "beatles") };
        const FilterExpr startsWithThe{ name(StringField::ArtistSortName, StringOperator::StartsWith, "the") };
        const FilterExpr startsWithAnim{ name(StringField::ArtistSortName, StringOperator::StartsWith, "anim") };

        EXPECT_EQ(findTrackIds(session, startsWithBeatles), (std::vector{ beatlesTrack.getId() }));
        EXPECT_TRUE(findTrackIds(session, startsWithThe).empty());
        // falls back on the names
        EXPECT_EQ(findTrackIds(session, startsWithAnim), (std::vector{ animalsTrack.getId() }));

        EXPECT_EQ(findArtistIds(session, startsWithBeatles), (std::vector{ beatles.getId() }));
        EXPECT_TRUE(findArtistIds(session, startsWithThe).empty());
        EXPECT_EQ(findArtistIds(session, startsWithAnim), (std::vector{ animals.getId() }));
    }

    TEST_F(DatabaseFixture, FilterExpr_nameFilterLiteralCharacters)
    {
        ScopedRelease percent{ session, "100% Hits" };
        ScopedRelease noPercent{ session, "100 Hits" };
        ScopedRelease underscore{ session, "a_b" };
        ScopedRelease noUnderscore{ session, "axb" };
        ScopedRelease backslash{ session, R"(C:\Music)" };
        ScopedRelease noBackslash{ session, "C:Music" };

        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseName, StringOperator::Equal, "100% Hits")), (std::vector{ percent.getId() }));
        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseName, StringOperator::StartsWith, "100%")), (std::vector{ percent.getId() }));
        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseName, StringOperator::Contains, "a_b")), (std::vector{ underscore.getId() }));
        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseName, StringOperator::Equal, R"(C:\Music)")), (std::vector{ backslash.getId() }));
        EXPECT_EQ(findReleaseIds(session, name(StringField::ReleaseName, StringOperator::EndsWith, R"(:\Music)")), (std::vector{ backslash.getId() }));
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
            session.create<TrackArtistLink>(track.get(), trackArtist.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(release.get(), releaseArtist.get(), false);
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
            session.create<TrackArtistLink>(trackInLibrary1.get(), artist.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(trackInLibrary2.get(), artist.get(), TrackArtistLinkType::Composer);
            session.create<ReleaseArtistLink>(release.get(), releaseArtist.get(), false);
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
        // releaseArtist is never returned when a link type is set: it has no credited track
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

            // artistA: credited on a 2020 track, and release artist of a 2022 release
            setUpTrack(trackA.get(), date2020);
            session.create<TrackArtistLink>(trackA.get(), artistA.get(), TrackArtistLinkType::Artist);

            setUpTrack(trackB.get(), date2022);
            trackB.get().modify()->setRelease(releaseB.get());
            session.create<TrackArtistLink>(trackB.get(), artistC.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(releaseB.get(), artistA.get(), false);

            // artistV: release artist only
            setUpTrack(trackV.get(), date2021);
            trackV.get().modify()->setRelease(releaseV.get());
            session.create<TrackArtistLink>(trackV.get(), artistD.get(), TrackArtistLinkType::Artist);
            session.create<ReleaseArtistLink>(releaseV.get(), artistV.get(), false);

            auto star{ [&](Artist::pointer artist, const Wt::WDateTime& dateTime) {
                auto feedback{ session.create<ArtistFeedback>(artist, user.get()) };
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
            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Artist);
            session.create<TrackArtistLink>(track.get(), artist.get(), TrackArtistLinkType::Composer);
            session.create<TrackArtistLink>(otherTrack.get(), otherArtist.get(), TrackArtistLinkType::Artist);
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
