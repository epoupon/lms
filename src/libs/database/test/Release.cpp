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

#include <array>

#include "Common.hpp"

#include "core/PartialDateTime.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Types.hpp"

namespace lms::db::tests
{
    using ScopedArtwork = ScopedEntity<db::Artwork>;
    using ScopedCountry = ScopedEntity<db::Country>;
    using ScopedImage = ScopedEntity<db::Image>;
    using ScopedLabel = ScopedEntity<db::Label>;
    using ScopedMedium = ScopedEntity<db::Medium>;
    using ScopedReleaseType = ScopedEntity<db::ReleaseType>;

    namespace
    {
        FilterExpr yearBetween(IntegerField field, std::int64_t min, std::int64_t max)
        {
            return FilterExpr::condition(IntegerCondition{ field, FilterOperator::Between, FilterRange<std::int64_t>{ min, max } });
        }
    } // namespace

    TEST_F(DatabaseFixture, Release)
    {
        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Release::getCount(session), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}), 0);
            EXPECT_FALSE(Release::exists(session, 0));
            EXPECT_FALSE(Release::exists(session, 1));
        }

        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Release::getCount(session), 1);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}), 1);
            EXPECT_TRUE(Release::exists(session, release.getId()));

            {
                const auto releases{ Release::findOrphanIds(session) };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases.front(), release.getId());
            }

            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}) };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases.front(), release.getId());
                EXPECT_EQ(Release::getDuration(session, release.getId()), std::chrono::seconds{ 0 });
            }

            {
                bool visited{};
                Release::find(session, Release::FindParameters{}, [&](const Release& r) {
                    visited = true;
                    EXPECT_EQ(r.getId(), release.getId());
                });
                EXPECT_TRUE(visited);
            }
        }
    }

    TEST_F(DatabaseFixture, Release_findByRangedIdBased)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2a{ session };
        ScopedTrack track2b{ session };
        ScopedTrack track3{ session };
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        ScopedRelease release3{ session, "MyRelease3" };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack2a{ track2a.get() };
            updatedTrack2a->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack2a);
            std::optional<Track> updatedTrack2b{ track2b.get() };
            updatedTrack2b->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack2b);
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);
            updatedTrack2a = track2a.get();
            updatedTrack2a->setRelease(release2.getId());
            Track::update(session, *updatedTrack2a);
            updatedTrack2b = track2b.get();
            updatedTrack2b->setRelease(release2.getId());
            Track::update(session, *updatedTrack2b);
            std::optional<Track> updatedTrack3{ track3.get() };
            updatedTrack3->setRelease(release3.getId());
            Track::update(session, *updatedTrack3);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId;
            std::vector<Release> visitedReleases;
            Release::find(session, lastRetrievedId, 10, [&](const Release& release) {
                visitedReleases.push_back(release);
            });
            ASSERT_EQ(visitedReleases.size(), 3);
            EXPECT_EQ(visitedReleases[0].getId(), release1.getId());
            EXPECT_EQ(visitedReleases[1].getId(), release2.getId());
            EXPECT_EQ(visitedReleases[2].getId(), release3.getId());
            EXPECT_EQ(lastRetrievedId, release3.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId{ release1.getId() };
            std::vector<Release> visitedReleases;
            Release::find(session, lastRetrievedId, 1, [&](const Release& release) {
                visitedReleases.push_back(release);
            });
            ASSERT_EQ(visitedReleases.size(), 1);
            EXPECT_EQ(visitedReleases[0].getId(), release2.getId());
            EXPECT_EQ(lastRetrievedId, release2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId{ release1.getId() };
            std::vector<Release> visitedReleases;
            Release::find(session, lastRetrievedId, 0, [&](const Release& release) {
                visitedReleases.push_back(release);
            });
            ASSERT_EQ(visitedReleases.size(), 0);
            EXPECT_EQ(lastRetrievedId, release1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId;
            std::vector<Release> visitedReleases;
            Release::find(
                session, lastRetrievedId, 10, [&](const Release& release) {
                    visitedReleases.push_back(release);
                },
                otherLibrary.getId());
            ASSERT_EQ(visitedReleases.size(), 0);
            EXPECT_EQ(lastRetrievedId, ReleaseId{});
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId;
            std::vector<Release> visitedReleases;
            Release::find(
                session, lastRetrievedId, 10, [&](const Release& release) {
                    visitedReleases.push_back(release);
                },
                library.getId());
            ASSERT_EQ(visitedReleases.size(), 1);
            EXPECT_EQ(visitedReleases[0].getId(), release2.getId());
            EXPECT_EQ(lastRetrievedId, release2.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_findNextIdRange)
    {
        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Release::findNextIdRange(session, ReleaseId{}, 0) };
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, ReleaseId{});
            EXPECT_EQ(range.last, ReleaseId{});

            range = Release::findNextIdRange(session, ReleaseId{}, 100);
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, ReleaseId{});
            EXPECT_EQ(range.last, ReleaseId{});
        }

        ScopedRelease release1{ session, "Artist1" };
        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Release::findNextIdRange(session, ReleaseId{}, 0) };
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, ReleaseId{});
            EXPECT_EQ(range.last, ReleaseId{});

            range = Release::findNextIdRange(session, ReleaseId{}, 1);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, release1.getId());
            EXPECT_EQ(range.last, release1.getId());

            range = Release::findNextIdRange(session, range.last, 1);
            EXPECT_FALSE(range.isValid());
            EXPECT_EQ(range.first, ReleaseId{});
            EXPECT_EQ(range.last, ReleaseId{});

            range = Release::findNextIdRange(session, ReleaseId{}, 100);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, release1.getId());
            EXPECT_EQ(range.last, release1.getId());
        }

        ScopedRelease release2{ session, "Artist2" };
        ScopedRelease release3{ session, "Artist3" };

        {
            auto transaction{ session.createReadTransaction() };

            auto range{ Release::findNextIdRange(session, ReleaseId{}, 2) };
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, release1.getId());
            EXPECT_EQ(range.last, release2.getId());

            range = Release::findNextIdRange(session, release2.getId(), 2);
            EXPECT_TRUE(range.isValid());
            EXPECT_EQ(range.first, release3.getId());
            EXPECT_EQ(range.last, release3.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_findByRange)
    {
        ScopedRelease release1{ session, "Artist1" };
        ScopedRelease release2{ session, "Artist2" };
        ScopedRelease release3{ session, "Artist3" };

        {
            auto transaction{ session.createReadTransaction() };

            std::size_t count{};
            Release::find(session, IdRange<ReleaseId>{ .first = release1.getId(), .last = release1.getId() }, [&](const db::Release& release) {
                count++;
                EXPECT_EQ(release.getId(), release1.getId());
            });
            EXPECT_EQ(count, 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::size_t count{};
            Release::find(session, IdRange<ReleaseId>{ .first = release1.getId(), .last = release3.getId() }, [&](const db::Release&) {
                count++;
            });
            EXPECT_EQ(count, 3);
        }
    }

    TEST_F(DatabaseFixture, Release_singleTrack)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            ScopedTrack track{ session };
            {
                auto transaction{ session.createWriteTransaction() };

                std::optional<Track> updatedTrack{ track.get() };
                updatedTrack->setRelease(release.getId());
                updatedTrack->setName("MyTrackName");
                Track::update(session, *updatedTrack);
                std::optional<Release> updatedRelease{ release.get() };
                updatedRelease->setName("MyReleaseName");
                Release::update(session, *updatedRelease);
            }

            {
                auto transaction{ session.createReadTransaction() };
                EXPECT_EQ(Release::findOrphanIds(session).size(), 0);

                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release.getId())) };
                ASSERT_EQ(tracks.size(), 1);
                EXPECT_EQ(tracks.front(), track.getId());
            }

            {
                auto transaction{ session.createWriteTransaction() };

                ASSERT_TRUE(track->getReleaseId().isValid());
                EXPECT_EQ(track->getReleaseId(), release.getId());
            }

            {
                auto transaction{ session.createWriteTransaction() };
                auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackName").setReleaseName("MyReleaseName")) };
                ASSERT_EQ(tracks.size(), 1);
                EXPECT_EQ(tracks.front(), track.getId());
            }
            {
                auto transaction{ session.createWriteTransaction() };
                auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackName").setReleaseName("MyReleaseFoo")) };
                EXPECT_EQ(tracks.size(), 0);
            }
            {
                auto transaction{ session.createWriteTransaction() };
                auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackFoo").setReleaseName("MyReleaseName")) };
                EXPECT_EQ(tracks.size(), 0);
            }
        }

        {
            auto transaction{ session.createWriteTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release.getId())) };
            EXPECT_EQ(tracks.size(), 0);

            auto releases{ Release::findOrphanIds(session) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_singleTrack_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            updatedTrack->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack);
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(FilterExpr::equal(library->getId()))) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(FilterExpr::equal(otherLibrary->getId()))) };
            EXPECT_EQ(releases.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, MulitpleReleaseSearchByName)
    {
        ScopedRelease release1{ session, "MyRelease" };
        ScopedRelease release2{ session, "MyRelease%" };
        ScopedRelease release3{ session, "%MyRelease" };
        ScopedRelease release4{ session, "MyRelease%Foo" };
        ScopedRelease release5{ session, "Foo%MyRelease" };
        ScopedRelease release6{ session, "_yRelease" };

        // filters does not work on orphans
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedTrack track4{ session };
        ScopedTrack track5{ session };
        ScopedTrack track6{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release2.getId());
            Track::update(session, *updatedTrack2);
            std::optional<Track> updatedTrack3{ track3.get() };
            updatedTrack3->setRelease(release3.getId());
            Track::update(session, *updatedTrack3);
            std::optional<Track> updatedTrack4{ track4.get() };
            updatedTrack4->setRelease(release4.getId());
            Track::update(session, *updatedTrack4);
            std::optional<Track> updatedTrack5{ track5.get() };
            updatedTrack5->setRelease(release5.getId());
            Track::update(session, *updatedTrack5);
            std::optional<Track> updatedTrack6{ track6.get() };
            updatedTrack6->setRelease(release6.getId());
            Track::update(session, *updatedTrack6);
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "Release" })) };
                EXPECT_EQ(releases.size(), 6);
            }

            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "MyRelease" })) };
                ASSERT_EQ(releases.size(), 5);
                EXPECT_TRUE(std::none_of(std::cbegin(releases), std::cend(releases), [&](const ReleaseId releaseId) { return releaseId == release6.getId(); }));
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "MyRelease%" })) };
                ASSERT_EQ(releases.size(), 2);
                EXPECT_EQ(releases[0], release2.getId());
                EXPECT_EQ(releases[1], release4.getId());
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "%MyRelease" })) };
                ASSERT_EQ(releases.size(), 2);
                EXPECT_EQ(releases[0], release3.getId());
                EXPECT_EQ(releases[1], release5.getId());
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "Foo%MyRelease" })) };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases[0], release5.getId());
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "MyRelease%Foo" })) };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases[0], release4.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, MulitpleReleaseSearchByNameWithBackslash)
    {
        ScopedRelease release1{ session, R"(AC\DC)" };
        ScopedRelease release2{ session, "ACDC" };
        ScopedRelease release3{ session, R"(MyRelease\)" };

        // filters does not work on orphans
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release2.getId());
            Track::update(session, *updatedTrack2);
            std::optional<Track> updatedTrack3{ track3.get() };
            updatedTrack3->setRelease(release3.getId());
            Track::update(session, *updatedTrack3);
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ R"(AC\DC)" })) };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases[0], release1.getId());
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ R"(MyRelease\)" })) };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases[0], release3.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, ReleaseSearchByMediumName)
    {
        ScopedRelease release{ session, "The Beatles In Mono" };
        ScopedMedium medium{ session, release.getId() };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            updatedTrack->setMedium(medium.getId());
            Track::update(session, *updatedTrack);

            std::optional<Medium> updatedMedium{ medium.get() };
            updatedMedium->setName("Sgt. Pepper's Lonely Hearts Club Band");
            Medium::update(session, *updatedMedium);
        }

        {
            auto transaction{ session.createReadTransaction() };

            // matches only the medium (discsubtitle), not the release name
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "Sgt. Pepper" })) };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases.front(), release.getId());
            }
            // release name search still works
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "Beatles In Mono" })) };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases.front(), release.getId());
            }
            // no match on either field
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "NoSuchKeyword" })) };
                EXPECT_EQ(releases.size(), 0);
            }
        }
    }

    TEST_F(DatabaseFixture, ReleaseSearchByMediumName_multipleMediaNoDuplicates)
    {
        ScopedRelease release{ session, "The Beatles In Mono" };
        ScopedMedium medium1{ session, release.getId() };
        ScopedMedium medium2{ session, release.getId() };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release.getId());
            updatedTrack1->setMedium(medium1.getId());
            Track::update(session, *updatedTrack1);

            std::optional<Medium> updatedMedium1{ medium1.get() };
            updatedMedium1->setName("Sgt. Pepper's Lonely Hearts Club Band");
            Medium::update(session, *updatedMedium1);

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release.getId());
            updatedTrack2->setMedium(medium2.getId());
            Track::update(session, *updatedTrack2);

            std::optional<Medium> updatedMedium2{ medium2.get() };
            updatedMedium2->setName("Abbey Road");
            Medium::update(session, *updatedMedium2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "Sgt. Pepper" })) };
            ASSERT_EQ(releases.size(), 1); // not duplicated despite the 1:N join
            EXPECT_EQ(releases.front(), release.getId());
        }
    }

    TEST_F(DatabaseFixture, MultiTracksSingleReleaseTotalDiscTrack)
    {
        ScopedRelease release1{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_FALSE(release1->getTotalDisc());
        }

        ScopedTrack track1{ session };
        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_FALSE(release1->getTotalDisc());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Release> updatedRelease1{ release1.get() };
            updatedRelease1->setTotalDisc(6);
            Release::update(session, *updatedRelease1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ASSERT_TRUE(release1->getTotalDisc());
            EXPECT_EQ(*release1->getTotalDisc(), 6);
        }

        ScopedTrack track2{ session };
        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release1.getId());
            Track::update(session, *updatedTrack2);
            std::optional<Release> updatedRelease1{ release1.get() };
            updatedRelease1->setTotalDisc(67);
            Release::update(session, *updatedRelease1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ASSERT_TRUE(release1->getTotalDisc());
            EXPECT_EQ(*release1->getTotalDisc(), 67);
        }

        ScopedRelease release2{ session, "MyRelease2" };
        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_FALSE(release2->getTotalDisc());
        }

        ScopedTrack track3{ session };
        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack3{ track3.get() };
            updatedTrack3->setRelease(release2.getId());
            Track::update(session, *updatedTrack3);
            std::optional<Release> updatedRelease2{ release2.get() };
            updatedRelease2->setTotalDisc(5);
            Release::update(session, *updatedRelease2);
        }
        {
            auto transaction{ session.createReadTransaction() };

            ASSERT_TRUE(release1->getTotalDisc());
            EXPECT_EQ(*release2->getTotalDisc(), 5);
            ASSERT_TRUE(release2->getTotalDisc());
            EXPECT_EQ(*release2->getTotalDisc(), 5);
        }
    }

    TEST_F(DatabaseFixture, MultiTracksSingleReleaseFirstTrack)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedMedium medium1A{ session, release1.getId() };
        ScopedMedium medium1B{ session, release1.getId() };
        ScopedRelease release2{ session, "MyRelease2" };
        ScopedMedium medium2A{ session, release2.getId() };
        ScopedMedium medium2B{ session, release2.getId() };

        ScopedTrack track1A{ session };
        ScopedTrack track1B{ session };
        ScopedTrack track2A{ session };
        ScopedTrack track2B{ session };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Track::findIds(session, Track::FindParameters{}.setRelease(release1.getId())).size(), 0);
            EXPECT_EQ(Track::findIds(session, Track::FindParameters{}.setRelease(release2.getId())).size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1A{ track1A.get() };
            updatedTrack1A->setRelease(release1.getId());
            updatedTrack1A->setMedium(medium1A.getId());
            Track::update(session, *updatedTrack1A);

            std::optional<Track> updatedTrack1B{ track1B.get() };
            updatedTrack1B->setRelease(release1.getId());
            updatedTrack1B->setMedium(medium1B.getId());
            Track::update(session, *updatedTrack1B);

            std::optional<Track> updatedTrack2A{ track2A.get() };
            updatedTrack2A->setRelease(release2.getId());
            updatedTrack2A->setMedium(medium2A.getId());
            Track::update(session, *updatedTrack2A);
            std::optional<Track> updatedTrack2B{ track2B.get() };
            updatedTrack2B->setRelease(release2.getId());
            updatedTrack2B->setMedium(medium2B.getId());
            Track::update(session, *updatedTrack2B);

            updatedTrack1A = track1A.get();
            updatedTrack1A->setTrackNumber(1);
            Track::update(session, *updatedTrack1A);
            updatedTrack1B = track1B.get();
            updatedTrack1B->setTrackNumber(2);
            Track::update(session, *updatedTrack1B);

            updatedTrack2A = track2A.get();
            updatedTrack2A->setTrackNumber(1);
            Track::update(session, *updatedTrack2A);
            updatedTrack2B = track2B.get();
            updatedTrack2B->setTrackNumber(2);
            Track::update(session, *updatedTrack2B);
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release1.getId()).setSortMethod(TrackSortMethod::Release)) };
                ASSERT_EQ(tracks.size(), 2);
                EXPECT_EQ(tracks[0], track1A.getId());
                EXPECT_EQ(tracks[1], track1B.getId());
            }

            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release2.getId()).setSortMethod(TrackSortMethod::Release)) };
                ASSERT_EQ(tracks.size(), 2);
                EXPECT_EQ(tracks[0], track2A.getId());
                EXPECT_EQ(tracks[1], track2B.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, MultiTracksSingleReleaseDate)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        const core::PartialDateTime release1Date{ 1994, 2, 3 };
        const core::PartialDateTime release1OriginalDate{ 1993, 4, 5 };

        ScopedTrack track1A{ session };
        ScopedTrack track1B{ session };
        ScopedTrack track2A{ session };
        ScopedTrack track2B{ session };

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::Year, -3000, 3000))) };
            EXPECT_EQ(releases.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1A{ track1A.get() };
            updatedTrack1A->setRelease(release1.getId());
            Track::update(session, *updatedTrack1A);
            std::optional<Track> updatedTrack1B{ track1B.get() };
            updatedTrack1B->setRelease(release1.getId());
            Track::update(session, *updatedTrack1B);
            std::optional<Track> updatedTrack2A{ track2A.get() };
            updatedTrack2A->setRelease(release2.getId());
            Track::update(session, *updatedTrack2A);
            std::optional<Track> updatedTrack2B{ track2B.get() };
            updatedTrack2B->setRelease(release2.getId());
            Track::update(session, *updatedTrack2B);

            updatedTrack1A = track1A.get();
            updatedTrack1A->setDate(release1Date);
            Track::update(session, *updatedTrack1A);
            updatedTrack1B = track1B.get();
            updatedTrack1B->setDate(release1Date);
            Track::update(session, *updatedTrack1B);
            updatedTrack1A = track1A.get();
            updatedTrack1A->setOriginalDate(release1OriginalDate);
            Track::update(session, *updatedTrack1A);
            updatedTrack1B = track1B.get();
            updatedTrack1B->setOriginalDate(release1OriginalDate);
            Track::update(session, *updatedTrack1B);

            EXPECT_EQ(Release::getDate(session, release1.getId()), release1Date);
            EXPECT_EQ(Release::getOriginalDate(session, release1.getId()), release1OriginalDate);

            EXPECT_EQ(Release::getYear(session, release1.getId()), release1Date.getYear());
            EXPECT_EQ(Release::getOriginalYear(session, release1.getId()), release1OriginalDate.getYear());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::Year, 1950, 2000))) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::Year, 1994, 1994)));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::Year, 1993, 1993)));
            ASSERT_EQ(releases.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, MultiTracksSingleReleaseOriginalDate)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        const core::PartialDateTime release1Date{ 1994, 2, 3 };
        const core::PartialDateTime release1OriginalDate{ 1993, 4, 5 };

        ScopedTrack track1A{ session };
        ScopedTrack track1B{ session };
        ScopedTrack track2A{ session };
        ScopedTrack track2B{ session };

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::OriginalYear, -3000, 3000))) };
            EXPECT_EQ(releases.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1A{ track1A.get() };
            updatedTrack1A->setRelease(release1.getId());
            Track::update(session, *updatedTrack1A);
            std::optional<Track> updatedTrack1B{ track1B.get() };
            updatedTrack1B->setRelease(release1.getId());
            Track::update(session, *updatedTrack1B);
            std::optional<Track> updatedTrack2A{ track2A.get() };
            updatedTrack2A->setRelease(release2.getId());
            Track::update(session, *updatedTrack2A);
            std::optional<Track> updatedTrack2B{ track2B.get() };
            updatedTrack2B->setRelease(release2.getId());
            Track::update(session, *updatedTrack2B);

            updatedTrack1A = track1A.get();
            updatedTrack1A->setDate(release1Date);
            Track::update(session, *updatedTrack1A);
            updatedTrack1B = track1B.get();
            updatedTrack1B->setDate(release1Date);
            Track::update(session, *updatedTrack1B);

            updatedTrack1A = track1A.get();
            updatedTrack1A->setOriginalDate(release1OriginalDate);
            Track::update(session, *updatedTrack1A);
            updatedTrack1B = track1B.get();
            updatedTrack1B->setOriginalDate(release1OriginalDate);
            Track::update(session, *updatedTrack1B);

            EXPECT_EQ(Release::getOriginalDate(session, release1.getId()), release1OriginalDate);
            EXPECT_EQ(Release::getOriginalYear(session, release1.getId()), release1OriginalDate.getYear());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases = Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::OriginalYear, 1950, 2000)));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::OriginalYear, 1993, 1993)));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::OriginalYear, 1994, 1994)));
            ASSERT_EQ(releases.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, MultiTracksSingleReleaseYear)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        const int release1Year{ 1994 };
        const int release1OriginalYear{ 1993 };

        ScopedTrack track1A{ session };
        ScopedTrack track1B{ session };
        ScopedTrack track2A{ session };
        ScopedTrack track2B{ session };

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::Year, 0, 3000))) };
            EXPECT_EQ(releases.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1A{ track1A.get() };
            updatedTrack1A->setRelease(release1.getId());
            Track::update(session, *updatedTrack1A);
            std::optional<Track> updatedTrack1B{ track1B.get() };
            updatedTrack1B->setRelease(release1.getId());
            Track::update(session, *updatedTrack1B);
            std::optional<Track> updatedTrack2A{ track2A.get() };
            updatedTrack2A->setRelease(release2.getId());
            Track::update(session, *updatedTrack2A);
            std::optional<Track> updatedTrack2B{ track2B.get() };
            updatedTrack2B->setRelease(release2.getId());
            Track::update(session, *updatedTrack2B);

            updatedTrack1A = track1A.get();
            updatedTrack1A->setDate(core::PartialDateTime{ release1Year });
            Track::update(session, *updatedTrack1A);
            updatedTrack1B = track1B.get();
            updatedTrack1B->setDate(core::PartialDateTime{ release1Year });
            Track::update(session, *updatedTrack1B);
            updatedTrack1A = track1A.get();
            updatedTrack1A->setOriginalDate(core::PartialDateTime{ release1OriginalYear });
            Track::update(session, *updatedTrack1A);
            updatedTrack1B = track1B.get();
            updatedTrack1B->setOriginalDate(core::PartialDateTime{ release1OriginalYear });
            Track::update(session, *updatedTrack1B);

            EXPECT_EQ(Release::getYear(session, release1.getId()), release1Year);
            EXPECT_EQ(Release::getOriginalYear(session, release1.getId()), release1OriginalYear);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::Year, 1950, 2000))) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::Year, 1994, 1994)));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setFilter(yearBetween(IntegerField::Year, 1993, 1993)));
            ASSERT_EQ(releases.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Release_trackArtist)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedArtist artist2{ session, "MyArtist2" };
        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Artist })) };
            EXPECT_EQ(releases.size(), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Artist })), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist.getId())), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist2.getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(releases.size(), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist2.getId(), { TrackArtistLinkType::Artist })), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist2.getId())), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Producer);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}), 1);

            auto releases{ Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Artist })) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Artist })), 1);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Remixer })), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Artist, TrackArtistLinkType::Mixer }));
            EXPECT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Artist, TrackArtistLinkType::Mixer })), 1);

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist2.getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(releases.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist2.getId()));
            EXPECT_EQ(releases.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Writer, TrackArtistLinkType::Artist }));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId()));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist.getId())), 1);
            EXPECT_EQ(Artist::find(session, Artist::FindParameters{}.setRelease(release.getId()).setTrackArtistLinkType(TrackArtistLinkType::Artist)).size(), 1);
            EXPECT_EQ(Artist::find(session, Artist::FindParameters{}.setRelease(release.getId()).setTrackArtistLinkType(TrackArtistLinkType::Artist)).size(), 1);
            EXPECT_EQ(Artist::find(session, Artist::FindParameters{}.setRelease(release.getId()).setTrackArtistLinkType(TrackArtistLinkType::Conductor)).size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Composer }));
            EXPECT_EQ(releases.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId(), { TrackArtistLinkType::Composer, TrackArtistLinkType::Mixer }));
            EXPECT_EQ(releases.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                bool visited{};
                TrackArtistLink::find(session, TrackArtistLink::FindParameters{}.setRelease(release.getId()).setLinkType(TrackArtistLinkType::Artist), [&](const db::TrackArtistLink& link) {
                    visited = true;
                    EXPECT_EQ(link.getArtistId(), artist.getId());
                });
                EXPECT_TRUE(visited);
            }

            {
                bool visited{};
                TrackArtistLink::find(session, TrackArtistLink::FindParameters{}.setRelease(release.getId()).setLinkType(TrackArtistLinkType::Conductor), [&](const db::TrackArtistLink&) {
                    visited = true;
                });
                EXPECT_FALSE(visited);
            }
        }
    }

    TEST_F(DatabaseFixture, Release_artist)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedArtist artist1{ session, "MyArtist" };
        ScopedArtist artist2{ session, "MyArtist" };
        ScopedArtist artist3{ session, "MyArtist" };

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setArtist(artist1.getId())) };
            EXPECT_EQ(releases.size(), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist1.getId())), 0);
            EXPECT_EQ(Artist::find(session, Artist::FindParameters{}.setRelease(release.getId()).setTrackArtistLinkType(TrackArtistLinkType::Conductor)).size(), 0);
            bool visited{};
            ReleaseArtistLink::find(session, release.getId(), [&](const db::ReleaseArtistLink&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            auto link1{ ReleaseArtistLink::create(session, release.getId(), artist1.getId(), false) };
            ReleaseArtistLink link2{ ReleaseArtistLink::create(session, release.getId(), artist2.getId(), true) };
            link2.setArtistName("MyArtistAlternateName");
            link2.setArtistSortName("MyArtistAlternateSortName");
            ReleaseArtistLink::update(session, link2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ReleaseArtistLink> links;
            ReleaseArtistLink::find(session, release.getId(), [&](const ReleaseArtistLink& link) {
                links.push_back(link);
            });
            ASSERT_EQ(links.size(), 2);
            EXPECT_EQ(links[0].getArtistId(), artist1.getId());
            EXPECT_EQ(links[0].getArtistName(), "");
            EXPECT_EQ(links[0].getArtistSortName(), "");
            EXPECT_EQ(links[1].getArtistId(), artist2.getId());
            EXPECT_EQ(links[1].getArtistName(), "MyArtistAlternateName");
            EXPECT_EQ(links[1].getArtistSortName(), "MyArtistAlternateSortName");
            EXPECT_EQ(links[1].isArtistMBIDMatched(), true);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<db::ReleaseArtistLink> links;
            ReleaseArtistLink::find(session, release.getId(), [&](const db::ReleaseArtistLink& artistLink) {
                links.push_back(artistLink);
            });
            ASSERT_EQ(links.size(), 2);
            EXPECT_EQ(links[0].getArtistId(), artist1.getId());
            EXPECT_EQ(links[0].getArtistName(), "");
            EXPECT_EQ(links[0].getArtistSortName(), "");
            EXPECT_EQ(links[1].getArtistId(), artist2.getId());
            EXPECT_EQ(links[1].getArtistName(), "MyArtistAlternateName");
            EXPECT_EQ(links[1].getArtistSortName(), "MyArtistAlternateSortName");
            EXPECT_EQ(links[1].isArtistMBIDMatched(), true);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}), 1);

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setArtist(artist1.getId())) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist1.getId())), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist1.getId())), 1);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setTrackArtist(artist2.getId())), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist2.getId())), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto artists{ Artist::find(session, Artist::FindParameters{}.setRelease(release.getId()).setReleaseArtistsOnly(true)) };
            EXPECT_EQ(artists.size(), 2);
            EXPECT_EQ(artists[0].getId(), artist1.getId());
            EXPECT_EQ(artists[1].getId(), artist2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_TRUE(Release::hasArtist(session, release.getId(), artist1.getId()));
            EXPECT_FALSE(Release::hasArtist(session, release.getId(), artist3.getId()));
        }
    }

    TEST_F(DatabaseFixture, Release_isCompilation)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(release.get()->isCompilation());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Release> updatedRelease{ release.get() };
            updatedRelease->setCompilation(true);
            Release::update(session, *updatedRelease);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_TRUE(release.get()->isCompilation());
        }
    }

    TEST_F(DatabaseFixture, Label)
    {
        {
            auto transaction{ session.createReadTransaction() };
            std::optional<Label> res{ Label::find(session, "label") };
            EXPECT_FALSE(res);
        }

        ScopedLabel label{ session, "MyLabel" };

        {
            auto transaction{ session.createReadTransaction() };
            std::optional<Label> res{ Label::find(session, "MyLabel") };
            ASSERT_TRUE(res);
            EXPECT_EQ(res->getId(), label.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_getLabelNames)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedLabel label{ session, "MyLabel" };

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            Label::find(session, release.getId(), [&](const Label&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Release::addLabel(session, release.getId(), label.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            std::vector<std::string> names;
            Label::find(session, release.getId(), [&](const Label& releaseLabel) {
                names.emplace_back(releaseLabel.getName());
            });
            ASSERT_EQ(names.size(), 1);
            EXPECT_EQ(names[0], "MyLabel");
        }
    }

    TEST_F(DatabaseFixture, Label_orphan)
    {
        ScopedLabel label{ session, "MyLabel" };

        {
            auto transaction{ session.createReadTransaction() };
            auto labels{ Label::findOrphanIds(session) };
            ASSERT_EQ(labels.size(), 1);
            EXPECT_EQ(labels.front(), label.getId());
        }

        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::addLabel(session, release.getId(), label.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto labels{ Label::findOrphanIds(session) };
            EXPECT_EQ(labels.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Release::clearLabels(session, release.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto labels{ Label::findOrphanIds(session) };
            ASSERT_EQ(labels.size(), 1);
            EXPECT_EQ(labels.front(), label.getId());
        }
    }

    TEST_F(DatabaseFixture, Country)
    {
        {
            auto transaction{ session.createReadTransaction() };
            std::optional<Country> res{ Country::find(session, "country") };
            EXPECT_FALSE(res);
        }

        ScopedCountry country{ session, "MyCountry" };

        {
            auto transaction{ session.createReadTransaction() };
            std::optional<Country> res{ Country::find(session, "MyCountry") };
            ASSERT_TRUE(res);
            EXPECT_EQ(res->getId(), country.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_getCountryNames)
    {
        ScopedCountry country{ session, "MyCountry" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::addCountry(session, release.getId(), country.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            std::vector<std::string> names;
            Country::find(session, release.getId(), [&](const Country& releaseCountry) {
                names.emplace_back(releaseCountry.getName());
            });
            ASSERT_EQ(names.size(), 1);
            EXPECT_EQ(names[0], "MyCountry");
        }
    }

    TEST_F(DatabaseFixture, Country_orphan)
    {
        ScopedCountry country{ session, "MyCountry" };

        {
            auto transaction{ session.createReadTransaction() };
            auto countries{ Country::findOrphanIds(session) };
            ASSERT_EQ(countries.size(), 1);
            EXPECT_EQ(countries.front(), country.getId());
        }

        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::addCountry(session, release.getId(), country.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto countries{ Country::findOrphanIds(session) };
            EXPECT_EQ(countries.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Release::clearCountries(session, release.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto countries{ Country::findOrphanIds(session) };
            ASSERT_EQ(countries.size(), 1);
            EXPECT_EQ(countries.front(), country.getId());
        }
    }

    TEST_F(DatabaseFixture, ReleaseType)
    {
        {
            auto transaction{ session.createReadTransaction() };
            std::optional<ReleaseType> res{ ReleaseType::find(session, "album") };
            EXPECT_FALSE(res);
        }

        ScopedReleaseType releaseType{ session, "album" };

        {
            auto transaction{ session.createReadTransaction() };
            std::optional<ReleaseType> res{ ReleaseType::find(session, "album") };
            ASSERT_TRUE(res);
            EXPECT_EQ(res->getId(), releaseType.getId());
        }
    }

    TEST_F(DatabaseFixture, ReleaseType_orphan)
    {
        // Orphan tests
        ScopedReleaseType releaseType{ session, "album" };

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseTypes{ ReleaseType::findOrphanIds(session) };
            ASSERT_EQ(releaseTypes.size(), 1);
            EXPECT_EQ(releaseTypes.front(), releaseType.getId());
        }

        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::addReleaseType(session, release.getId(), releaseType.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseTypes{ ReleaseType::findOrphanIds(session) };
            EXPECT_EQ(releaseTypes.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Release::clearReleaseTypes(session, release.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseTypes{ ReleaseType::findOrphanIds(session) };
            ASSERT_EQ(releaseTypes.size(), 1);
            EXPECT_EQ(releaseTypes.front(), releaseType.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_releaseType)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            bool visited{};
            ReleaseType::find(session, release.getId(), [&](const ReleaseType&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        ScopedReleaseType releaseType{ session, "album" };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::addReleaseType(session, release.getId(), releaseType.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ReleaseType> releaseTypes;
            ReleaseType::find(session, release.getId(), [&](const ReleaseType& type) {
                releaseTypes.push_back(type);
            });
            ASSERT_EQ(releaseTypes.size(), 1);
            EXPECT_EQ(releaseTypes.front().getId(), releaseType.getId());
            EXPECT_EQ(releaseTypes.front().getName(), "album");
        }
    }

    TEST_F(DatabaseFixture, Release_find_releaseType)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            auto releases{ Release::findIds(session, Release::FindParameters{}.setReleaseType("Foo")) };
            EXPECT_EQ(releases.size(), 0);
        }

        ScopedReleaseType releaseType{ session, "album" };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::addReleaseType(session, release.getId(), releaseType.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setReleaseType("Foo")) };
            EXPECT_EQ(releases.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setReleaseType("album"));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortMethod)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        const core::PartialDateTime release1Date{ 2000, 2, 3 };
        const core::PartialDateTime release1OriginalDate{ 1993, 4, 5 };

        ScopedRelease release2{ session, "MyRelease2" };
        const core::PartialDateTime release2Date{ 1994, 2, 3 };

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        ASSERT_LT(release2Date, release1Date);
        ASSERT_GT(release2Date, release1OriginalDate);

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            updatedTrack1->setOriginalDate(release1OriginalDate);
            updatedTrack1->setDate(release1Date);
            Track::update(session, *updatedTrack1);

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release2.getId());
            updatedTrack2->setDate(release2Date);
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::Name)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases.front(), release1.getId());
            EXPECT_EQ(releases.back(), release2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::Random)) };
            ASSERT_EQ(releases.size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::DateAsc)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases.front(), release2.getId());
            EXPECT_EQ(releases.back(), release1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::DateDesc)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases.front(), release1.getId());
            EXPECT_EQ(releases.back(), release2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::OriginalDate)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases.front(), release1.getId());
            EXPECT_EQ(releases.back(), release2.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::OriginalDateDesc)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases.front(), release2.getId());
            EXPECT_EQ(releases.back(), release1.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortDate_multiTrackAggregation)
    {
        // release1 has 2 tracks with widely different dates; release2 has 1 track in between.
        // DateAsc/DateDesc must both key off release1's earliest track (MIN), not its latest,
        // and DateDesc must be the exact reverse of DateAsc rather than re-keying by a different track.
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        ScopedTrack track1Early{ session };
        ScopedTrack track1Late{ session };
        ScopedTrack track2{ session };

        const core::PartialDateTime release1EarlyDate{ 1980, 1, 1 };
        const core::PartialDateTime release1LateDate{ 2010, 1, 1 };
        const core::PartialDateTime release2Date{ 1994, 2, 3 };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1Early{ track1Early.get() };
            updatedTrack1Early->setDate(release1EarlyDate);
            updatedTrack1Early->setRelease(release1.getId());
            Track::update(session, *updatedTrack1Early);
            std::optional<Track> updatedTrack1Late{ track1Late.get() };
            updatedTrack1Late->setDate(release1LateDate);
            updatedTrack1Late->setRelease(release1.getId());
            Track::update(session, *updatedTrack1Late);

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setDate(release2Date);
            updatedTrack2->setRelease(release2.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releasesAsc{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::DateAsc)) };
            ASSERT_EQ(releasesAsc.size(), 2);
            EXPECT_EQ(releasesAsc[0], release1.getId());
            EXPECT_EQ(releasesAsc[1], release2.getId());

            const auto releasesDesc{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::DateDesc)) };
            ASSERT_EQ(releasesDesc.size(), 2);
            EXPECT_EQ(releasesDesc[0], release2.getId());
            EXPECT_EQ(releasesDesc[1], release1.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortOriginalDate_multiTrackAggregation)
    {
        // Same idea as Release_sortDate_multiTrackAggregation, but for COALESCE(original_date, date)
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        ScopedTrack track1Early{ session };
        ScopedTrack track1Late{ session };
        ScopedTrack track2{ session };

        const core::PartialDateTime release1EarlyOriginalDate{ 1980, 1, 1 };
        const core::PartialDateTime release1LateOriginalDate{ 2010, 1, 1 };
        const core::PartialDateTime release2Date{ 1994, 2, 3 }; // no original date -> falls back to date

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack1Early{ track1Early.get() };
            updatedTrack1Early->setOriginalDate(release1EarlyOriginalDate);
            updatedTrack1Early->setRelease(release1.getId());
            Track::update(session, *updatedTrack1Early);
            std::optional<Track> updatedTrack1Late{ track1Late.get() };
            updatedTrack1Late->setOriginalDate(release1LateOriginalDate);
            updatedTrack1Late->setRelease(release1.getId());
            Track::update(session, *updatedTrack1Late);

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setDate(release2Date);
            updatedTrack2->setRelease(release2.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releasesAsc{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::OriginalDate)) };
            ASSERT_EQ(releasesAsc.size(), 2);
            EXPECT_EQ(releasesAsc[0], release1.getId());
            EXPECT_EQ(releasesAsc[1], release2.getId());

            const auto releasesDesc{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::OriginalDateDesc)) };
            ASSERT_EQ(releasesDesc.size(), 2);
            EXPECT_EQ(releasesDesc[0], release2.getId());
            EXPECT_EQ(releasesDesc[1], release1.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortDateAsc_tieBreakOnEffectiveSortName)
    {
        ScopedRelease releaseZulu{ session, "Zulu" };   // no sort name -> falls back on "Zulu"
        ScopedRelease releaseAlpha{ session, "Alpha" }; // explicit sort name -> "Aardvark"

        ScopedTrack trackZulu{ session };
        ScopedTrack trackAlpha{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Release> updatedReleaseAlpha{ releaseAlpha.get() };
            updatedReleaseAlpha->setSortName("Aardvark");
            Release::update(session, *updatedReleaseAlpha);

            const core::PartialDateTime date{ 2000, 1, 1 };
            std::optional<Track> updatedTrackZulu{ trackZulu.get() };
            updatedTrackZulu->setDate(date);
            updatedTrackZulu->setRelease(releaseZulu.getId());
            Track::update(session, *updatedTrackZulu);
            std::optional<Track> updatedTrackAlpha{ trackAlpha.get() };
            updatedTrackAlpha->setDate(date);
            updatedTrackAlpha->setRelease(releaseAlpha.getId());
            Track::update(session, *updatedTrackAlpha);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::DateAsc)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], releaseAlpha.getId());
            EXPECT_EQ(releases[1], releaseZulu.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortArtistNameThenName)
    {
        ScopedRelease releaseZulu{ session, "Zulu" };      // artist "Zeppelin" -> primary key "Zeppelin"
        ScopedRelease releaseAlpha{ session, "Alpha" };    // artist "Abba" -> primary key "Abba"
        ScopedRelease releaseNoArtist{ session, "Bravo" }; // no artist display name -> falls back on release name "Bravo"

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Release> updatedReleaseZulu{ releaseZulu.get() };
            updatedReleaseZulu->setArtistDisplayName("Zeppelin");
            Release::update(session, *updatedReleaseZulu);
            std::optional<Release> updatedReleaseAlpha{ releaseAlpha.get() };
            updatedReleaseAlpha->setArtistDisplayName("Abba");
            Release::update(session, *updatedReleaseAlpha);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::ArtistNameThenName)) };
            ASSERT_EQ(releases.size(), 3);
            EXPECT_EQ(releases[0], releaseAlpha.getId());    // "Abba"
            EXPECT_EQ(releases[1], releaseNoArtist.getId()); // "Bravo" (fallback on release name)
            EXPECT_EQ(releases[2], releaseZulu.getId());     // "Zeppelin"
        }
    }

    TEST_F(DatabaseFixture, Release_sortArtistNameThenName_tieBreakOnEffectiveSortName)
    {
        // Two releases sharing the same artist display name must then tie-break on the release's own effective sort name
        ScopedRelease releaseZulu{ session, "Zulu" };   // no sort name -> falls back on "Zulu"
        ScopedRelease releaseAlpha{ session, "Alpha" }; // explicit sort name -> "Aardvark"

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Release> updatedReleaseZulu{ releaseZulu.get() };
            updatedReleaseZulu->setArtistDisplayName("SameArtist");
            Release::update(session, *updatedReleaseZulu);
            std::optional<Release> updatedReleaseAlpha{ releaseAlpha.get() };
            updatedReleaseAlpha->setArtistDisplayName("SameArtist");
            updatedReleaseAlpha->setSortName("Aardvark");
            Release::update(session, *updatedReleaseAlpha);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::ArtistNameThenName)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], releaseAlpha.getId());
            EXPECT_EQ(releases[1], releaseZulu.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_meanBitrate)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        auto checkExpectedBitrate = [&](std::size_t bitrate) {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Release::getMeanBitrate(session, release1.getId()), bitrate);
        };

        checkExpectedBitrate(0);

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setBitrate(128);
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);
        }

        checkExpectedBitrate(128);

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setBitrate(256);
            updatedTrack2->setRelease(release1.getId());
            Track::update(session, *updatedTrack2);
        }
        checkExpectedBitrate(192);

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack3{ track3.get() };
            updatedTrack3->setBitrate(0);
            updatedTrack3->setRelease(release1.getId());
            Track::update(session, *updatedTrack3);
        }
        checkExpectedBitrate(192); // 0 should not be taken into account
    }

    TEST_F(DatabaseFixture, Release_codec)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedTrack track4{ session };

        {
            auto transaction{ session.createReadTransaction() };
            ASSERT_EQ(Release::getCodecs(session, release1.getId()).size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setCodec(core::media::Codec::FLAC);
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setCodec(core::media::Codec::MP3);
            updatedTrack2->setRelease(release1.getId());
            Track::update(session, *updatedTrack2);

            std::optional<Track> updatedTrack3{ track3.get() };
            updatedTrack3->setCodec(core::media::Codec::MP3);
            updatedTrack3->setRelease(release1.getId());
            Track::update(session, *updatedTrack3);

            std::optional<Track> updatedTrack4{ track4.get() };
            updatedTrack4->setCodec(core::media::Codec::AAC);
            updatedTrack4->setRelease(release1.getId());
            Track::update(session, *updatedTrack4);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Release::getCodecs(session, release1.getId()), (std::vector<core::media::Codec>{ core::media::Codec::MP3, core::media::Codec::FLAC, core::media::Codec::AAC }));
        }
    }

    TEST_F(DatabaseFixture, Release_findBycodec)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedTrack track1{ session };

        ScopedRelease release2{ session, "MyRelease2" };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createReadTransaction() };
            ASSERT_EQ(Release::getCodecs(session, release1.getId()).size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setCodec(core::media::Codec::FLAC);
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setCodec(core::media::Codec::MP3);
            updatedTrack2->setRelease(release2.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Release::FindParameters params;
            params.setFilter(FilterExpr::equal(core::media::Codec::MP3));

            auto releases{ Release::findIds(session, params) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release2.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_trackCount)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        ScopedRelease release3{ session, "MyRelease2" };

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release1.getId());
            Track::update(session, *updatedTrack2);
            std::optional<Track> updatedTrack3{ track3.get() };
            updatedTrack3->setRelease(release2.getId());
            Track::update(session, *updatedTrack3);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Release::getTrackCount(session, release1.getId()), 2);
            EXPECT_EQ(Release::getTrackCount(session, release2.getId()), 1);
            EXPECT_EQ(Release::getTrackCount(session, release3.getId()), 0);
        }
    }

    TEST_F(DatabaseFixture, Release_artwork)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(release->getPreferredArtworkId().isValid());
        }

        ScopedImage image{ session, "/image.jpg" };
        ScopedArtwork artwork{ session, image.getId() };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::updatePreferredArtwork(session, release.getId(), artwork.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release.get()->getPreferredArtworkId(), artwork.getId());
        }

        // Check cascade delete
        {
            auto transaction{ session.createWriteTransaction() };
            Image::destroy(session, std::array{ image.getId() });
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(release.get()->getPreferredArtworkId().isValid());
        }
    }

    TEST_F(DatabaseFixture, Release_sortDateAdded)
    {
        ScopedRelease releaseA{ session, "relA" };
        ScopedRelease releaseB{ session, "relB" };
        ScopedRelease releaseC{ session, "relC" };
        ScopedRelease releaseD{ session, "relD" };

        ScopedTrack trackA1{ session };
        ScopedTrack trackB1{ session };
        ScopedTrack trackC1{ session };
        ScopedTrack trackD1{ session };

        ScopedTrack trackA2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrackA1{ trackA1.get() };
            updatedTrackA1->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            Track::update(session, *updatedTrackA1);
            std::optional<Track> updatedTrackB1{ trackB1.get() };
            updatedTrackB1->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            Track::update(session, *updatedTrackB1);
            std::optional<Track> updatedTrackD1{ trackD1.get() };
            updatedTrackD1->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            updatedTrackD1->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 3 } });
            Track::update(session, *updatedTrackD1);
            std::optional<Track> updatedTrackA2{ trackA2.get() };
            updatedTrackA2->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 4 } });
            Track::update(session, *updatedTrackA2);

            updatedTrackA1 = trackA1.get();
            updatedTrackA1->setRelease(releaseA.getId());
            Track::update(session, *updatedTrackA1);
            updatedTrackA2 = trackA2.get();
            updatedTrackA2->setRelease(releaseA.getId());
            Track::update(session, *updatedTrackA2);
            updatedTrackB1 = trackB1.get();
            updatedTrackB1->setRelease(releaseB.getId());
            Track::update(session, *updatedTrackB1);
            std::optional<Track> updatedTrackC1{ trackC1.get() };
            updatedTrackC1->setRelease(releaseC.getId());
            Track::update(session, *updatedTrackC1);
            updatedTrackD1 = trackD1.get();
            updatedTrackD1->setRelease(releaseD.getId());
            Track::update(session, *updatedTrackD1);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::AddedDesc)) };
            ASSERT_EQ(releases.size(), 4);
            // releaseA's earliest track (2021-01-02) is what counts for "added", not its latest (2021-01-04):
            // a release shouldn't jump to the top just because one track's tag was rescanned later
            EXPECT_EQ(releases[0], releaseD.getId());
            EXPECT_EQ(releases[1], releaseA.getId());
            EXPECT_EQ(releases[2], releaseB.getId());
            EXPECT_EQ(releases[3], releaseC.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortDateAdded_tieBreakOnEffectiveSortName)
    {
        ScopedRelease releaseZulu{ session, "Zulu" };   // no sort name -> falls back on "Zulu"
        ScopedRelease releaseAlpha{ session, "Alpha" }; // explicit sort name -> "Aardvark"

        ScopedTrack trackZulu{ session };
        ScopedTrack trackAlpha{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Release> updatedReleaseAlpha{ releaseAlpha.get() };
            updatedReleaseAlpha->setSortName("Aardvark");
            Release::update(session, *updatedReleaseAlpha);

            const Wt::WDateTime addedTime{ Wt::WDate{ 2021, 1, 2 } };
            std::optional<Track> updatedTrackZulu{ trackZulu.get() };
            updatedTrackZulu->setAddedTime(addedTime);
            Track::update(session, *updatedTrackZulu);
            std::optional<Track> updatedTrackAlpha{ trackAlpha.get() };
            updatedTrackAlpha->setAddedTime(addedTime);
            Track::update(session, *updatedTrackAlpha);

            updatedTrackZulu = trackZulu.get();
            updatedTrackZulu->setRelease(releaseZulu.getId());
            Track::update(session, *updatedTrackZulu);
            updatedTrackAlpha = trackAlpha.get();
            updatedTrackAlpha->setRelease(releaseAlpha.getId());
            Track::update(session, *updatedTrackAlpha);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::AddedDesc)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], releaseAlpha.getId());
            EXPECT_EQ(releases[1], releaseZulu.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortLastWritten)
    {
        ScopedRelease releaseA{ session, "relA" };
        ScopedRelease releaseB{ session, "relB" };
        ScopedRelease releaseC{ session, "relC" };
        ScopedRelease releaseD{ session, "relD" };

        ScopedTrack trackA1{ session };
        ScopedTrack trackB1{ session };
        ScopedTrack trackC1{ session };
        ScopedTrack trackD1{ session };

        ScopedTrack trackA2{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrackA1{ trackA1.get() };
            updatedTrackA1->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            Track::update(session, *updatedTrackA1);
            std::optional<Track> updatedTrackB1{ trackB1.get() };
            updatedTrackB1->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            Track::update(session, *updatedTrackB1);
            std::optional<Track> updatedTrackD1{ trackD1.get() };
            updatedTrackD1->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            updatedTrackD1->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 3 } });
            Track::update(session, *updatedTrackD1);
            std::optional<Track> updatedTrackA2{ trackA2.get() };
            updatedTrackA2->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 4 } });
            Track::update(session, *updatedTrackA2);

            updatedTrackA1 = trackA1.get();
            updatedTrackA1->setRelease(releaseA.getId());
            Track::update(session, *updatedTrackA1);
            updatedTrackA2 = trackA2.get();
            updatedTrackA2->setRelease(releaseA.getId());
            Track::update(session, *updatedTrackA2);
            updatedTrackB1 = trackB1.get();
            updatedTrackB1->setRelease(releaseB.getId());
            Track::update(session, *updatedTrackB1);
            std::optional<Track> updatedTrackC1{ trackC1.get() };
            updatedTrackC1->setRelease(releaseC.getId());
            Track::update(session, *updatedTrackC1);
            updatedTrackD1 = trackD1.get();
            updatedTrackD1->setRelease(releaseD.getId());
            Track::update(session, *updatedTrackD1);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::LastWrittenDesc)) };
            ASSERT_EQ(releases.size(), 4);
            EXPECT_EQ(releases[0], releaseA.getId());
            EXPECT_EQ(releases[1], releaseD.getId());
            EXPECT_EQ(releases[2], releaseB.getId());
            EXPECT_EQ(releases[3], releaseC.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortLastWritten_tieBreakOnEffectiveSortName)
    {
        ScopedRelease releaseZulu{ session, "Zulu" };   // no sort name -> falls back on "Zulu"
        ScopedRelease releaseAlpha{ session, "Alpha" }; // explicit sort name -> "Aardvark"

        ScopedTrack trackZulu{ session };
        ScopedTrack trackAlpha{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Release> updatedReleaseAlpha{ releaseAlpha.get() };
            updatedReleaseAlpha->setSortName("Aardvark");
            Release::update(session, *updatedReleaseAlpha);

            const Wt::WDateTime lastWriteTime{ Wt::WDate{ 2021, 1, 2 } };
            std::optional<Track> updatedTrackZulu{ trackZulu.get() };
            updatedTrackZulu->setLastWriteTime(lastWriteTime);
            Track::update(session, *updatedTrackZulu);
            std::optional<Track> updatedTrackAlpha{ trackAlpha.get() };
            updatedTrackAlpha->setLastWriteTime(lastWriteTime);
            Track::update(session, *updatedTrackAlpha);

            updatedTrackZulu = trackZulu.get();
            updatedTrackZulu->setRelease(releaseZulu.getId());
            Track::update(session, *updatedTrackZulu);
            updatedTrackAlpha = trackAlpha.get();
            updatedTrackAlpha->setRelease(releaseAlpha.getId());
            Track::update(session, *updatedTrackAlpha);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::LastWrittenDesc)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], releaseAlpha.getId());
            EXPECT_EQ(releases[1], releaseZulu.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_LastWritten)
    {
        ScopedRelease release{ session, "relA" };

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createReadTransaction() };

            Wt::WDateTime lastWritten{ Release::getLastWrittenTime(session, release.getId()) };
            EXPECT_FALSE(lastWritten.isValid());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            Track::update(session, *updatedTrack2);
            updatedTrack1 = track1.get();
            updatedTrack1->setRelease(release.getId());
            Track::update(session, *updatedTrack1);
            updatedTrack2 = track2.get();
            updatedTrack2->setRelease(release.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Wt::WDateTime lastWritten{ Release::getLastWrittenTime(session, release.getId()) };
            ASSERT_TRUE(lastWritten.isValid());
            EXPECT_EQ(lastWritten, track2.get()->getLastWriteTime());
        }
    }

    TEST_F(DatabaseFixture, Release_AddedTime)
    {
        ScopedRelease release{ session, "relA" };

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createReadTransaction() };

            const Wt::WDateTime addedTime{ Release::getAddedTime(session, release.getId()) };
            EXPECT_FALSE(addedTime.isValid());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            Track::update(session, *updatedTrack2);
            updatedTrack1 = track1.get();
            updatedTrack1->setRelease(release.getId());
            Track::update(session, *updatedTrack1);
            updatedTrack2 = track2.get();
            updatedTrack2->setRelease(release.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const Wt::WDateTime addedTime{ Release::getAddedTime(session, release.getId()) };
            ASSERT_TRUE(addedTime.isValid());
            EXPECT_EQ(addedTime, track2.get()->getAddedTime());
        }
    }

    TEST_F(DatabaseFixture, Release_groupMBID)
    {
        ScopedRelease release{ session, "relA" };
        const std::optional<core::UUID> groupMBID{ core::UUID::fromString("1ad8f716-2fd6-4d09-8ada-39525947217c") };

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setReleaseGroupMBID(groupMBID)) };
            EXPECT_EQ(releases.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Release> updatedRelease{ release.get() };
            updatedRelease->setGroupMBID(groupMBID);
            Release::update(session, *updatedRelease);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setReleaseGroupMBID(groupMBID)) };
            EXPECT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release->getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortName)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Release> updatedRelease1{ release1.get() };
            updatedRelease1->setSortName("BB");
            Release::update(session, *updatedRelease1);
            std::optional<Release> updatedRelease2{ release2.get() };
            updatedRelease2->setSortName("AA");
            Release::update(session, *updatedRelease2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::Name)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], release1->getId());
            EXPECT_EQ(releases[1], release2->getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::SortName)) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], release2->getId());
            EXPECT_EQ(releases[1], release1->getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortName_emptyFallsBackOnName)
    {
        ScopedRelease releaseBravo{ session, "Bravo" };     // no sort name -> falls back on "Bravo"
        ScopedRelease releaseAlpha{ session, "Alpha" };     // explicit sort name -> "Zulu"
        ScopedRelease releaseCharlie{ session, "Charlie" }; // no sort name -> falls back on "Charlie"

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Release> updatedReleaseAlpha{ releaseAlpha.get() };
            updatedReleaseAlpha->setSortName("Zulu");
            Release::update(session, *updatedReleaseAlpha);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::SortName)) };

            ASSERT_EQ(releases.size(), 3);
            EXPECT_EQ(releases[0], releaseBravo.getId());
            EXPECT_EQ(releases[1], releaseCharlie.getId());
            EXPECT_EQ(releases[2], releaseAlpha.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_updateArtwork)
    {
        ScopedRelease release{ session, "MyRelease" };
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(release->getPreferredArtworkId().isValid());
        }

        ScopedImage image{ session, "/image1.jpg" };
        ScopedArtwork artwork{ session, image.getId() };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::updatePreferredArtwork(session, release->getId(), artwork.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release->getPreferredArtworkId(), artwork.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Release::updatePreferredArtwork(session, release.getId(), ArtworkId{});
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(release->getPreferredArtworkId().isValid());
        }
    }

    TEST_F(DatabaseFixture, Release_mediums)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            Medium::find(session, Medium::FindParameters{}.setRelease(release.getId()), [&](const Medium&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        ScopedMedium medium2{ session, release.getId() };

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<Medium> mediums;
            Medium::find(session, Medium::FindParameters{}.setRelease(release.getId()).setSortMethod(MediumSortMethod::PositionAsc), [&](const Medium& medium) {
                mediums.push_back(medium);
            });
            ASSERT_EQ(mediums.size(), 1);
            EXPECT_EQ(mediums[0].getId(), medium2.getId());
        }

        ScopedMedium medium1{ session, release.getId() };

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Medium> updatedMedium1{ medium1.get() };
            updatedMedium1->setPosition(1);
            Medium::update(session, *updatedMedium1);

            std::optional<Medium> updatedMedium2{ medium2.get() };
            updatedMedium2->setPosition(2);
            Medium::update(session, *updatedMedium2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<Medium> mediums;
            Medium::find(session, Medium::FindParameters{}.setRelease(release.getId()).setSortMethod(MediumSortMethod::PositionAsc), [&](const Medium& medium) {
                mediums.push_back(medium);
            });
            ASSERT_EQ(mediums.size(), 2);
            EXPECT_EQ(mediums[0].getId(), medium1.getId());
            EXPECT_EQ(mediums[1].getId(), medium2.getId());
        }
    }

} // namespace lms::db::tests