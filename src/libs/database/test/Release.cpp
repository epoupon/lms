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

#include "database/Image.hpp"

namespace lms::db::tests
{
    using ScopedImage = ScopedEntity<db::Image>;
    using ScopedLabel = ScopedEntity<db::Label>;
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
        ScopedMediaLibrary library{ session };
        ScopedMediaLibrary otherLibrary{ session };

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
        ScopedMediaLibrary library{ session };
        ScopedMediaLibrary otherLibrary{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setRelease(release.get());
            track.get().modify()->setMediaLibrary(library.get());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto releases{ Release::findIds(session, Release::FindParameters{}.setMediaLibrary(library->getId())) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };
            auto releases{ Release::findIds(session, Release::FindParameters{}.setMediaLibrary(otherLibrary->getId())) };
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

            track1.get().modify()->setTotalTrack(36);
            release1.get().modify()->setTotalDisc(6);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ASSERT_TRUE(track1->getTotalTrack());
            EXPECT_EQ(*track1->getTotalTrack(), 36);
            ASSERT_TRUE(release1->getTotalDisc());
            EXPECT_EQ(*release1->getTotalDisc(), 6);
        }

        ScopedTrack track2{ session };
        {
            auto transaction{ session.createWriteTransaction() };

            track2.get().modify()->setRelease(release1.get());
            track2.get().modify()->setTotalTrack(37);
            release1.get().modify()->setTotalDisc(67);
        }

        {
            auto transaction{ session.createReadTransaction() };

            ASSERT_TRUE(track1->getTotalTrack());
            EXPECT_EQ(*track1->getTotalTrack(), 36);
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
            track3.get().modify()->setTotalTrack(7);
            release2.get().modify()->setTotalDisc(5);
        }
        {
            auto transaction{ session.createReadTransaction() };

            ASSERT_TRUE(track1->getTotalTrack());
            EXPECT_EQ(*track1->getTotalTrack(), 36);
            ASSERT_TRUE(release1->getTotalDisc());
            EXPECT_EQ(*release2->getTotalDisc(), 5);
            ASSERT_TRUE(track3->getTotalTrack());
            EXPECT_EQ(*track3->getTotalTrack(), 7);
            ASSERT_TRUE(release2->getTotalDisc());
            EXPECT_EQ(*release2->getTotalDisc(), 5);
        }
    }

    TEST_F(DatabaseFixture, MultiTracksSingleReleaseFirstTrack)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

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
            track1B.get().modify()->setRelease(release1.get());
            track2A.get().modify()->setRelease(release2.get());
            track2B.get().modify()->setRelease(release2.get());

            track1A.get().modify()->setTrackNumber(1);
            track1B.get().modify()->setTrackNumber(2);

            track2A.get().modify()->setDiscNumber(2);
            track2A.get().modify()->setTrackNumber(1);
            track2B.get().modify()->setTrackNumber(2);
            track2B.get().modify()->setDiscNumber(1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release1.getId()).setSortMethod(TrackSortMethod::Release)) };
                EXPECT_EQ(tracks.results.size(), 2);
                EXPECT_EQ(tracks.results.front(), track1A.getId());
            }

            {
                const auto tracks{ Track::findIds(session, Track::FindParameters{}.setRelease(release2.getId()).setSortMethod(TrackSortMethod::Release)) };
                EXPECT_EQ(tracks.results.size(), 2);
                EXPECT_EQ(tracks.results.front(), track2B.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, MultiTracksSingleReleaseDate)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        const Wt::WDate release1Date{ Wt::WDate{ 1994, 2, 3 } };
        const Wt::WDate release1OriginalDate{ Wt::WDate{ 1993, 4, 5 } };

        ScopedTrack track1A{ session };
        ScopedTrack track1B{ session };
        ScopedTrack track2A{ session };
        ScopedTrack track2B{ session };

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setDateRange(DateRange::fromYearRange(0, 3000))) };
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
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setDateRange(DateRange::fromYearRange(1950, 2000))) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setDateRange(DateRange::fromYearRange(1994, 1994)));
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setDateRange(DateRange::fromYearRange(1993, 1993)));
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

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setDateRange(DateRange::fromYearRange(0, 3000))) };
            EXPECT_EQ(releases.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            track1A.get().modify()->setRelease(release1.get());
            track1B.get().modify()->setRelease(release1.get());
            track2A.get().modify()->setRelease(release2.get());
            track2B.get().modify()->setRelease(release2.get());

            track1A.get().modify()->setYear(release1Year);
            track1B.get().modify()->setYear(release1Year);
            track1A.get().modify()->setOriginalYear(release1OriginalYear);
            track1B.get().modify()->setOriginalYear(release1OriginalYear);

            EXPECT_EQ(release1.get()->getYear(), release1Year);
            EXPECT_EQ(release1.get()->getOriginalYear(), release1OriginalYear);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setDateRange(DateRange::fromYearRange(1950, 2000))) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setDateRange(DateRange::fromYearRange(1994, 1994)));
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release1.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setDateRange(DateRange::fromYearRange(1993, 1993)));
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

    TEST_F(DatabaseFixture, Release_getDiscCount)
    {
        ScopedRelease release{ session, "MyRelease" };
        ScopedTrack track{ session };
        ScopedTrack track2{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release.get()->getDiscCount(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release.get()->getDiscCount(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setDiscNumber(5);
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release.get()->getDiscCount(), 1);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            track2.get().modify()->setRelease(release.get());
            track2.get().modify()->setDiscNumber(5);
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release.get()->getDiscCount(), 1);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            track2.get().modify()->setDiscNumber(6);
        }
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release.get()->getDiscCount(), 2);
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
        const Wt::WDate release1Date{ Wt::WDate{ 2000, 2, 3 } };
        const Wt::WDate release1OriginalDate{ Wt::WDate{ 1993, 4, 5 } };

        ScopedRelease release2{ session, "MyRelease2" };
        const Wt::WDate release2Date{ Wt::WDate{ 1994, 2, 3 } };

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

    TEST_F(DatabaseFixture, Release_image)
    {
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_FALSE(release.get()->getImage());
        }

        ScopedImage image{ session, "/myImage" };

        {
            auto transaction{ session.createWriteTransaction() };
            release.get().modify()->setImage(image.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto releaseImage(release.get()->getImage());
            ASSERT_TRUE(releaseImage);
            EXPECT_EQ(releaseImage->getId(), image.getId());
        }
    }
} // namespace lms::db::tests