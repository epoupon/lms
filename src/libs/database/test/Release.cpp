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

#include "Common.hpp"

#include "core/PartialDateTime.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Medium.hpp"

namespace lms::db::tests
{
    using ScopedArtwork = ScopedEntity<db::Artwork>;
    using ScopedCountry = ScopedEntity<db::Country>;
    using ScopedImage = ScopedEntity<db::Image>;
    using ScopedLabel = ScopedEntity<db::Label>;
    using ScopedMedium = ScopedEntity<db::Medium>;
    using ScopedReleaseType = ScopedEntity<db::ReleaseType>;

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
                ASSERT_EQ(releases.results.size(), 1);
                EXPECT_EQ(releases.results.front(), release.getId());
            }

            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}) };
                ASSERT_EQ(releases.results.size(), 1);
                EXPECT_EQ(releases.results.front(), release.getId());
                EXPECT_EQ(release->getDuration(), std::chrono::seconds{ 0 });
            }

            {
                const auto releases{ Release::find(session, Release::FindParameters{}) };
                ASSERT_EQ(releases.results.size(), 1);
                EXPECT_EQ(releases.results.front()->getId(), release.getId());
            }

            {
                bool visited{};
                Release::find(session, Release::FindParameters{}, [&](const Release::pointer& r) {
                    visited = true;
                    EXPECT_EQ(r->getId(), release.getId());
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
            track2a.get().modify()->setMediaLibrary(library.get());
            track2b.get().modify()->setMediaLibrary(library.get());
            track1.get().modify()->setRelease(release1.get());
            track2a.get().modify()->setRelease(release2.get());
            track2b.get().modify()->setRelease(release2.get());
            track3.get().modify()->setRelease(release3.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId;
            std::vector<Release::pointer> visitedReleases;
            Release::find(session, lastRetrievedId, 10, [&](const Release::pointer& release) {
                visitedReleases.push_back(release);
            });
            ASSERT_EQ(visitedReleases.size(), 3);
            EXPECT_EQ(visitedReleases[0]->getId(), release1.getId());
            EXPECT_EQ(visitedReleases[1]->getId(), release2.getId());
            EXPECT_EQ(visitedReleases[2]->getId(), release3.getId());
            EXPECT_EQ(lastRetrievedId, release3.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId{ release1.getId() };
            std::vector<Release::pointer> visitedReleases;
            Release::find(session, lastRetrievedId, 1, [&](const Release::pointer& release) {
                visitedReleases.push_back(release);
            });
            ASSERT_EQ(visitedReleases.size(), 1);
            EXPECT_EQ(visitedReleases[0]->getId(), release2.getId());
            EXPECT_EQ(lastRetrievedId, release2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId{ release1.getId() };
            std::vector<Release::pointer> visitedReleases;
            Release::find(session, lastRetrievedId, 0, [&](const Release::pointer& release) {
                visitedReleases.push_back(release);
            });
            ASSERT_EQ(visitedReleases.size(), 0);
            EXPECT_EQ(lastRetrievedId, release1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId;
            std::vector<Release::pointer> visitedReleases;
            Release::find(
                session, lastRetrievedId, 10, [&](const Release::pointer& release) {
                    visitedReleases.push_back(release);
                },
                otherLibrary.getId());
            ASSERT_EQ(visitedReleases.size(), 0);
            EXPECT_EQ(lastRetrievedId, ReleaseId{});
        }

        {
            auto transaction{ session.createReadTransaction() };

            ReleaseId lastRetrievedId;
            std::vector<Release::pointer> visitedReleases;
            Release::find(
                session, lastRetrievedId, 10, [&](const Release::pointer& release) {
                    visitedReleases.push_back(release);
                },
                library.getId());
            ASSERT_EQ(visitedReleases.size(), 1);
            EXPECT_EQ(visitedReleases[0]->getId(), release2.getId());
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
            Release::find(session, IdRange<ReleaseId>{ .first = release1.getId(), .last = release1.getId() }, [&](const db::Release::pointer& release) {
                count++;
                EXPECT_EQ(release->getId(), release1.getId());
            });
            EXPECT_EQ(count, 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::size_t count{};
            Release::find(session, IdRange<ReleaseId>{ .first = release1.getId(), .last = release3.getId() }, [&](const db::Release::pointer&) {
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

                track.get().modify()->setRelease(release.get());
                track.get().modify()->setName("MyTrackName");
                release.get().modify()->setName("MyReleaseName");
            }

            {
                auto transaction{ session.createReadTransaction() };
                EXPECT_EQ(Release::findOrphanIds(session).results.size(), 0);

                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release.getId())) };
                ASSERT_EQ(tracks.results.size(), 1);
                EXPECT_EQ(tracks.results.front(), track.getId());
            }

            {
                auto transaction{ session.createWriteTransaction() };

                ASSERT_TRUE(track->getRelease());
                EXPECT_EQ(track->getRelease()->getId(), release.getId());
            }

            {
                auto transaction{ session.createWriteTransaction() };
                auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackName").setReleaseName("MyReleaseName")) };
                ASSERT_EQ(tracks.results.size(), 1);
                EXPECT_EQ(tracks.results.front(), track.getId());
            }
            {
                auto transaction{ session.createWriteTransaction() };
                auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackName").setReleaseName("MyReleaseFoo")) };
                EXPECT_EQ(tracks.results.size(), 0);
            }
            {
                auto transaction{ session.createWriteTransaction() };
                auto tracks{ Track::findIds(session, Track::FindParameters{}.setName("MyTrackFoo").setReleaseName("MyReleaseName")) };
                EXPECT_EQ(tracks.results.size(), 0);
            }
        }

        {
            auto transaction{ session.createWriteTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release.getId())) };
            EXPECT_EQ(tracks.results.size(), 0);

            auto releases{ Release::findOrphanIds(session) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
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

            track.get().modify()->setRelease(release.get());
            track.get().modify()->setMediaLibrary(library.get());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto releases{ Release::findIds(session, Release::FindParameters{}.setFilters(Filters{}.setMediaLibrary(library->getId()))) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto releases{ Release::findIds(session, Release::FindParameters{}.setFilters(Filters{}.setMediaLibrary(otherLibrary->getId()))) };
            EXPECT_EQ(releases.results.size(), 0);
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

            track1.get().modify()->setRelease(release1.get());
            track2.get().modify()->setRelease(release2.get());
            track3.get().modify()->setRelease(release3.get());
            track4.get().modify()->setRelease(release4.get());
            track5.get().modify()->setRelease(release5.get());
            track6.get().modify()->setRelease(release6.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "Release" })) };
                EXPECT_EQ(releases.results.size(), 6);
            }

            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "MyRelease" })) };
                ASSERT_EQ(releases.results.size(), 5);
                EXPECT_TRUE(std::none_of(std::cbegin(releases.results), std::cend(releases.results), [&](const ReleaseId releaseId) { return releaseId == release6.getId(); }));
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "MyRelease%" })) };
                ASSERT_EQ(releases.results.size(), 2);
                EXPECT_EQ(releases.results[0], release2.getId());
                EXPECT_EQ(releases.results[1], release4.getId());
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "%MyRelease" })) };
                ASSERT_EQ(releases.results.size(), 2);
                EXPECT_EQ(releases.results[0], release3.getId());
                EXPECT_EQ(releases.results[1], release5.getId());
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "Foo%MyRelease" })) };
                ASSERT_EQ(releases.results.size(), 1);
                EXPECT_EQ(releases.results[0], release5.getId());
            }
            {
                const auto releases{ Release::findIds(session, Release::FindParameters{}.setKeywords({ "MyRelease%Foo" })) };
                ASSERT_EQ(releases.results.size(), 1);
                EXPECT_EQ(releases.results[0], release4.getId());
            }
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

            track1.get().modify()->setRelease(release1.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_FALSE(release1->getTotalDisc());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            release1.get().modify()->setTotalDisc(6);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ASSERT_TRUE(release1->getTotalDisc());
            EXPECT_EQ(*release1->getTotalDisc(), 6);
        }

        ScopedTrack track2{ session };
        {
            auto transaction{ session.createWriteTransaction() };

            track2.get().modify()->setRelease(release1.get());
            release1.get().modify()->setTotalDisc(67);
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

            track3.get().modify()->setRelease(release2.get());
            release2.get().modify()->setTotalDisc(5);
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
        ScopedMedium medium1A{ session, release1.lockAndGet() };
        ScopedMedium medium1B{ session, release1.lockAndGet() };
        ScopedRelease release2{ session, "MyRelease2" };
        ScopedMedium medium2A{ session, release2.lockAndGet() };
        ScopedMedium medium2B{ session, release2.lockAndGet() };

        ScopedTrack track1A{ session };
        ScopedTrack track1B{ session };
        ScopedTrack track2A{ session };
        ScopedTrack track2B{ session };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Track::findIds(session, Track::FindParameters{}.setRelease(release1.getId())).results.size(), 0);
            EXPECT_EQ(Track::findIds(session, Track::FindParameters{}.setRelease(release2.getId())).results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            track1A.get().modify()->setRelease(release1.get());
            track1A.get().modify()->setMedium(medium1A.get());

            track1B.get().modify()->setRelease(release1.get());
            track1B.get().modify()->setMedium(medium1B.get());

            track2A.get().modify()->setRelease(release2.get());
            track2A.get().modify()->setMedium(medium2A.get());
            track2B.get().modify()->setRelease(release2.get());
            track2B.get().modify()->setMedium(medium2B.get());

            track1A.get().modify()->setTrackNumber(1);
            track1B.get().modify()->setTrackNumber(2);

            track2A.get().modify()->setTrackNumber(1);
            track2B.get().modify()->setTrackNumber(2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release1.getId()).setSortMethod(TrackSortMethod::Release)) };
                ASSERT_EQ(tracks.results.size(), 2);
                EXPECT_EQ(tracks.results[0], track1A.getId());
                EXPECT_EQ(tracks.results[1], track1B.getId());
            }

            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release2.getId()).setSortMethod(TrackSortMethod::Release)) };
                ASSERT_EQ(tracks.results.size(), 2);
                EXPECT_EQ(tracks.results[0], track2A.getId());
                EXPECT_EQ(tracks.results[1], track2B.getId());
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

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setDateRange(YearRange{ -3000, 3000 })) };
            EXPECT_EQ(releases.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            track1A.get().modify()->setRelease(release1.get());
            track1B.get().modify()->setRelease(release1.get());
            track2A.get().modify()->setRelease(release2.get());
            track2B.get().modify()->setRelease(release2.get());

            track1A.get().modify()->setDate(release1Date);
            track1B.get().modify()->setDate(release1Date);
            track1A.get().modify()->setOriginalDate(release1OriginalDate);
            track1B.get().modify()->setOriginalDate(release1OriginalDate);

            EXPECT_EQ(release1.get()->getDate(), release1Date);
            EXPECT_EQ(release1.get()->getOriginalDate(), release1OriginalDate);

            EXPECT_EQ(release1.get()->getYear(), release1Date.getYear());
            EXPECT_EQ(release1.get()->getOriginalYear(), release1OriginalDate.getYear());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setDateRange(YearRange{ 1950, 2000 })) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setDateRange(YearRange{ 1994, 1994 }));
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setDateRange(YearRange{ 1993, 1993 }));
            ASSERT_EQ(releases.results.size(), 0);
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

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setDateRange(YearRange{ 0, 3000 })) };
            EXPECT_EQ(releases.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            track1A.get().modify()->setRelease(release1.get());
            track1B.get().modify()->setRelease(release1.get());
            track2A.get().modify()->setRelease(release2.get());
            track2B.get().modify()->setRelease(release2.get());

            track1A.get().modify()->setDate(core::PartialDateTime{ release1Year });
            track1B.get().modify()->setDate(core::PartialDateTime{ release1Year });
            track1A.get().modify()->setOriginalDate(core::PartialDateTime{ release1OriginalYear });
            track1B.get().modify()->setOriginalDate(core::PartialDateTime{ release1OriginalYear });

            EXPECT_EQ(release1.get()->getYear(), release1Year);
            EXPECT_EQ(release1.get()->getOriginalYear(), release1OriginalYear);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setDateRange(YearRange{ 1950, 2000 })) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setDateRange(YearRange{ 1994, 1994 }));
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setDateRange(YearRange{ 1993, 1993 }));
            ASSERT_EQ(releases.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Release_writtenAfter)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{ session };

        const Wt::WDateTime dateTime{ Wt::WDate{ 1950, 1, 1 }, Wt::WTime{ 12, 30, 20 } };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setLastWriteTime(dateTime);
            track.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto releases{ Release::findIds(session, Release::FindParameters{}) };
            EXPECT_EQ(releases.results.size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto releases{ Release::findIds(session, Release::FindParameters{}.setWrittenAfter(dateTime.addSecs(-1))) };
            EXPECT_EQ(releases.results.size(), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto releases{ Release::findIds(session, Release::FindParameters{}.setWrittenAfter(dateTime.addSecs(+1))) };
            EXPECT_EQ(releases.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Release_artist)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedArtist artist2{ session, "MyArtist2" };
        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Artist })) };
            EXPECT_EQ(releases.results.size(), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Artist })), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId())), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist2.getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(releases.results.size(), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist2.getId(), { TrackArtistLinkType::Artist })), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist2.getId())), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Producer);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}), 1);

            auto releases{ Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Artist })) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Artist })), 1);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Remixer })), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Artist, TrackArtistLinkType::Mixer }));
            EXPECT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Artist, TrackArtistLinkType::Mixer })), 1);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist2.getId(), { TrackArtistLinkType::Artist }));
            EXPECT_EQ(releases.results.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist2.getId()));
            EXPECT_EQ(releases.results.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::ReleaseArtist, TrackArtistLinkType::Artist }));
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId()));
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId())), 1);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Composer }));
            EXPECT_EQ(releases.results.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::Composer, TrackArtistLinkType::Mixer }));
            EXPECT_EQ(releases.results.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), {}, { TrackArtistLinkType::Artist }));
            EXPECT_EQ(releases.results.size(), 0);

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), {}, { TrackArtistLinkType::Artist, TrackArtistLinkType::Composer }));
            EXPECT_EQ(releases.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Release_releaseArtist)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::ReleaseArtist })) };
            EXPECT_EQ(releases.results.size(), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::ReleaseArtist })), 0);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId())), 0);
            EXPECT_EQ(release->getArtists(TrackArtistLinkType::ReleaseArtist).size(), 0);
            EXPECT_EQ(release->getArtistIds(TrackArtistLinkType::ReleaseArtist).size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::ReleaseArtist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ release->getArtists(TrackArtistLinkType::ReleaseArtist) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front()->getId(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ release->getArtistIds(TrackArtistLinkType::ReleaseArtist) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}), 1);

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::ReleaseArtist })) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId(), { TrackArtistLinkType::ReleaseArtist })), 1);
            EXPECT_EQ(Release::getCount(session, Release::FindParameters{}.setArtist(artist.getId())), 1);
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
            release.get().modify()->setCompilation(true);
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
            Label::pointer res{ Label::find(session, "label") };
            EXPECT_EQ(res, Label::pointer{});
        }

        ScopedLabel label{ session, "MyLabel" };

        {
            auto transaction{ session.createReadTransaction() };
            Label::pointer res{ Label::find(session, "MyLabel") };
            EXPECT_EQ(res, label.get());
        }
    }

    TEST_F(DatabaseFixture, Release_getLabelNames)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedLabel label{ session, "MyLabel" };

        {
            auto transaction{ session.createReadTransaction() };
            const auto names{ release.get()->getLabelNames() };
            EXPECT_EQ(names.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addLabel(label.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto names{ release.get()->getLabelNames() };
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
            ASSERT_EQ(labels.results.size(), 1);
            EXPECT_EQ(labels.results.front(), label.getId());
        }

        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addLabel(label.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto labels{ Label::findOrphanIds(session) };
            EXPECT_EQ(labels.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->clearLabels();
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto labels{ Label::findOrphanIds(session) };
            ASSERT_EQ(labels.results.size(), 1);
            EXPECT_EQ(labels.results.front(), label.getId());
        }
    }

    TEST_F(DatabaseFixture, Country)
    {
        {
            auto transaction{ session.createReadTransaction() };
            Country::pointer res{ Country::find(session, "country") };
            EXPECT_EQ(res, Country::pointer{});
        }

        ScopedCountry country{ session, "MyCountry" };

        {
            auto transaction{ session.createReadTransaction() };
            Country::pointer res{ Country::find(session, "MyCountry") };
            EXPECT_EQ(res, country.get());
        }
    }

    TEST_F(DatabaseFixture, Release_getCountryNames)
    {
        ScopedCountry country{ session, "MyCountry" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addCountry(country.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto names{ release.get()->getCountryNames() };
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
            ASSERT_EQ(countries.results.size(), 1);
            EXPECT_EQ(countries.results.front(), country.getId());
        }

        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addCountry(country.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto countries{ Country::findOrphanIds(session) };
            EXPECT_EQ(countries.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->clearCountries();
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto countries{ Country::findOrphanIds(session) };
            ASSERT_EQ(countries.results.size(), 1);
            EXPECT_EQ(countries.results.front(), country.getId());
        }
    }

    TEST_F(DatabaseFixture, ReleaseType)
    {
        {
            auto transaction{ session.createReadTransaction() };
            ReleaseType::pointer res{ ReleaseType::find(session, "album") };
            EXPECT_EQ(res, ReleaseType::pointer{});
        }

        ScopedReleaseType releaseType{ session, "album" };

        {
            auto transaction{ session.createReadTransaction() };
            ReleaseType::pointer res{ ReleaseType::find(session, "album") };
            EXPECT_EQ(res, releaseType.get());
        }
    }

    TEST_F(DatabaseFixture, ReleaseType_orphan)
    {
        // Orphan tests
        ScopedReleaseType releaseType{ session, "album" };

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseTypes{ ReleaseType::findOrphanIds(session) };
            ASSERT_EQ(releaseTypes.results.size(), 1);
            EXPECT_EQ(releaseTypes.results.front(), releaseType.getId());
        }

        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addReleaseType(releaseType.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseTypes{ ReleaseType::findOrphanIds(session) };
            EXPECT_EQ(releaseTypes.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->clearReleaseTypes();
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseTypes{ ReleaseType::findOrphanIds(session) };
            ASSERT_EQ(releaseTypes.results.size(), 1);
            EXPECT_EQ(releaseTypes.results.front(), releaseType.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_releaseType)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release.get()->getReleaseTypes().size(), 0);
        }

        ScopedReleaseType releaseType{ session, "album" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addReleaseType(releaseType.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releaseTypes{ release.get()->getReleaseTypes() };
            ASSERT_EQ(releaseTypes.size(), 1);
            EXPECT_EQ(releaseTypes.front()->getId(), releaseType.getId());

            const auto releaseTypeNames{ release.get()->getReleaseTypeNames() };
            ASSERT_EQ(releaseTypeNames.size(), 1);
            EXPECT_EQ(releaseTypeNames.front(), "album");
        }
    }

    TEST_F(DatabaseFixture, Release_find_releaseType)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            auto releases{ Release::find(session, Release::FindParameters{}.setReleaseType("Foo")).results };
            EXPECT_EQ(releases.size(), 0);
        }

        ScopedReleaseType releaseType{ session, "album" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->addReleaseType(releaseType.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::find(session, Release::FindParameters{}.setReleaseType("Foo")).results };
            EXPECT_EQ(releases.size(), 0);

            releases = Release::find(session, Release::FindParameters{}.setReleaseType("album")).results;
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front()->getId(), release.getId());
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

            track1.get().modify()->setRelease(release1.get());
            track1.get().modify()->setOriginalDate(release1OriginalDate);
            track1.get().modify()->setDate(release1Date);

            track2.get().modify()->setRelease(release2.get());
            track2.get().modify()->setDate(release2Date);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::Name)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results.front(), release1.getId());
            EXPECT_EQ(releases.results.back(), release2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::Random)) };
            ASSERT_EQ(releases.results.size(), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::DateAsc)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results.front(), release2.getId());
            EXPECT_EQ(releases.results.back(), release1.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::DateDesc)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results.front(), release1.getId());
            EXPECT_EQ(releases.results.back(), release2.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::OriginalDate)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results.front(), release1.getId());
            EXPECT_EQ(releases.results.back(), release2.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::OriginalDateDesc)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results.front(), release2.getId());
            EXPECT_EQ(releases.results.back(), release1.getId());
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
            EXPECT_EQ(release1->getMeanBitrate(), bitrate);
        };

        checkExpectedBitrate(0);

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setBitrate(128);
            track1.get().modify()->setRelease(release1.get());
        }

        checkExpectedBitrate(128);

        {
            auto transaction{ session.createWriteTransaction() };
            track2.get().modify()->setBitrate(256);
            track2.get().modify()->setRelease(release1.get());
        }
        checkExpectedBitrate(192);

        {
            auto transaction{ session.createWriteTransaction() };
            track3.get().modify()->setBitrate(0);
            track3.get().modify()->setRelease(release1.get());
        }
        checkExpectedBitrate(192); // 0 should not be taken into account
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
            track1.get().modify()->setRelease(release1.get());
            track2.get().modify()->setRelease(release1.get());
            track3.get().modify()->setRelease(release2.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release1->getTrackCount(), 2);
            EXPECT_EQ(release2->getTrackCount(), 1);
            EXPECT_EQ(release3->getTrackCount(), 0);
        }
    }

    TEST_F(DatabaseFixture, Release_artwork)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(release.get()->getPreferredArtwork());
        }

        ScopedImage image{ session, "/image.jpg" };
        ScopedArtwork artwork{ session, image.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->setPreferredArtwork(artwork.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseArtwork(release.get()->getPreferredArtwork());
            ASSERT_TRUE(releaseArtwork);
            EXPECT_EQ(releaseArtwork->getId(), artwork.getId());
        }

        // Check cascade delete
        {
            auto transaction{ session.createWriteTransaction() };
            image.lockAndGet().remove();
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseArtwork(release.get()->getPreferredArtwork());
            ASSERT_FALSE(releaseArtwork);
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
            trackA1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            trackB1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            trackD1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            trackD1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 3 } });
            trackA2.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 4 } });

            trackA1.get().modify()->setRelease(releaseA.get());
            trackA2.get().modify()->setRelease(releaseA.get());
            trackB1.get().modify()->setRelease(releaseB.get());
            trackC1.get().modify()->setRelease(releaseC.get());
            trackD1.get().modify()->setRelease(releaseD.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::AddedDesc)) };
            ASSERT_EQ(releases.results.size(), 4);
            EXPECT_EQ(releases.results[0], releaseA.getId());
            EXPECT_EQ(releases.results[1], releaseD.getId());
            EXPECT_EQ(releases.results[2], releaseB.getId());
            EXPECT_EQ(releases.results[3], releaseC.getId());
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
            trackA1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            trackB1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 1 } });
            trackD1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            trackD1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 3 } });
            trackA2.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 4 } });

            trackA1.get().modify()->setRelease(releaseA.get());
            trackA2.get().modify()->setRelease(releaseA.get());
            trackB1.get().modify()->setRelease(releaseB.get());
            trackC1.get().modify()->setRelease(releaseC.get());
            trackD1.get().modify()->setRelease(releaseD.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto releases{ Release::findIds(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::LastWrittenDesc)) };
            ASSERT_EQ(releases.results.size(), 4);
            EXPECT_EQ(releases.results[0], releaseA.getId());
            EXPECT_EQ(releases.results[1], releaseD.getId());
            EXPECT_EQ(releases.results[2], releaseB.getId());
            EXPECT_EQ(releases.results[3], releaseC.getId());
        }
    }

    TEST_F(DatabaseFixture, Release_LastWritten)
    {
        ScopedRelease release{ session, "relA" };

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createReadTransaction() };

            Wt::WDateTime lastWritten{ release.get()->getLastWrittenTime() };
            EXPECT_FALSE(lastWritten.isValid());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            track2.get().modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            track1.get().modify()->setRelease(release.get());
            track2.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Wt::WDateTime lastWritten{ release.get()->getLastWrittenTime() };
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

            const Wt::WDateTime addedTime{ release.get()->getAddedTime() };
            EXPECT_FALSE(addedTime.isValid());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 } });
            track2.get().modify()->setAddedTime(Wt::WDateTime{ Wt::WDate{ 2021, 1, 2 }, Wt::WTime{ 15, 36, 24 } });
            track1.get().modify()->setRelease(release.get());
            track2.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const Wt::WDateTime addedTime{ release.get()->getAddedTime() };
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

            const auto releases{ Release::find(session, Release::FindParameters{}.setReleaseGroupMBID(groupMBID)) };
            EXPECT_EQ(releases.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->setGroupMBID(groupMBID);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::find(session, Release::FindParameters{}.setReleaseGroupMBID(groupMBID)) };
            EXPECT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0]->getId(), release->getId());
        }
    }

    TEST_F(DatabaseFixture, Release_sortName)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        {
            auto transaction{ session.createWriteTransaction() };
            release1.get().modify()->setSortName("BB");
            release2.get().modify()->setSortName("AA");
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::find(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::Name)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0]->getId(), release1->getId());
            EXPECT_EQ(releases.results[1]->getId(), release2->getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::find(session, Release::FindParameters{}.setSortMethod(ReleaseSortMethod::SortName)) };
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0]->getId(), release2->getId());
            EXPECT_EQ(releases.results[1]->getId(), release1->getId());
        }
    }

    TEST_F(DatabaseFixture, Release_updateArtwork)
    {
        ScopedRelease release{ session, "MyRelease" };
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release->getPreferredArtwork(), Artwork::pointer{});
        }

        ScopedImage image{ session, "/image1.jpg" };
        ScopedArtwork artwork{ session, image.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };
            Release::updatePreferredArtwork(session, release->getId(), artwork.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release->getPreferredArtwork()->getId(), artwork.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Release::updatePreferredArtwork(session, release.getId(), ArtworkId{});
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release->getPreferredArtwork(), Artwork::pointer{});
        }
    }

    TEST_F(DatabaseFixture, Release_mediums)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };

            const auto mediums{ release->getMediums() };
            EXPECT_EQ(mediums.size(), 0);
        }

        ScopedMedium medium2{ session, release.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };

            const auto mediums{ release->getMediums() };
            ASSERT_EQ(mediums.size(), 1);
            EXPECT_EQ(mediums[0]->getId(), medium2.getId());
        }

        ScopedMedium medium1{ session, release.lockAndGet() };

        {
            auto transaction{ session.createWriteTransaction() };

            medium1.get().modify()->setPosition(1);
            medium2.get().modify()->setPosition(2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto mediums{ release->getMediums() };
            ASSERT_EQ(mediums.size(), 2);
            EXPECT_EQ(mediums[0]->getId(), medium1.getId());
            EXPECT_EQ(mediums[1]->getId(), medium2.getId());
        }
    }
} // namespace lms::db::tests