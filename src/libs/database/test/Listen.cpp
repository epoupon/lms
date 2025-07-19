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

#include "database/objects/Listen.hpp"

#include "Common.hpp"

namespace lms::db::tests
{
    using ScopedListen = ScopedEntity<db::Listen>;

    TEST_F(DatabaseFixture, Listen_getAll)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Listen::getCount(session), 0);
        }

        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } } };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Listen::getCount(session), 1);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            ScopedListen listen2{ session, user.get(), track.get(), ScrobblingBackend::Internal, Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 13, 0, 1 } } };

            EXPECT_EQ(Listen::getCount(session), 2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Listen::getCount(session), 1);
        }
    }

    TEST_F(DatabaseFixture, Listen_get)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } } };

        {
            auto transaction{ session.createReadTransaction() };

            auto listens{ Listen::find(session, Listen::FindParameters{}.setUser(user->getId()).setScrobblingBackend(ScrobblingBackend::ListenBrainz)) };
            EXPECT_EQ(listens.results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                auto listens{ Listen::find(session, Listen::FindParameters{}.setUser(user->getId()).setScrobblingBackend(ScrobblingBackend::Internal)) };
                EXPECT_EQ(listens.moreResults, false);
                ASSERT_EQ(listens.results.size(), 1);
                EXPECT_EQ(listens.results.front(), listen->getId());
            }

            {
                auto listens{ Listen::find(session, Listen::FindParameters{}.setUser(user->getId()).setScrobblingBackend(ScrobblingBackend::Internal).setSyncState(SyncState::PendingAdd)) };
                EXPECT_EQ(listens.results.size(), 1);
            }
            {
                auto listens{ Listen::find(session, Listen::FindParameters{}.setUser(user->getId()).setScrobblingBackend(ScrobblingBackend::Internal).setSyncState(SyncState::Synchronized)) };
                EXPECT_EQ(listens.results.size(), 0);
            }
        }
    }

    TEST_F(DatabaseFixture, Listen_get_multi)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedListen listen3{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 3 } } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } } };
        ScopedListen listen2{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 2 } } };

        {
            auto transaction{ session.createReadTransaction() };

            auto listens{ Listen::find(session, Listen::FindParameters{}.setUser(user->getId()).setScrobblingBackend(ScrobblingBackend::Internal)) };
            ASSERT_EQ(listens.results.size(), 3);
            EXPECT_EQ(listens.results[0], listen1.getId());
            EXPECT_EQ(listens.results[1], listen2.getId());
            EXPECT_EQ(listens.results[2], listen3.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_get_byDateTime)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        const Wt::WDateTime dateTime2{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 2 } };
        ASSERT_GT(dateTime2, dateTime1);

        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };
        ScopedListen listen2{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime2 };

        {
            auto transaction{ session.createReadTransaction() };

            {
                Listen::pointer listen{ Listen::find(session, user.getId(), track.getId(), ScrobblingBackend::Internal, dateTime1) };
                ASSERT_TRUE(listen);
                EXPECT_EQ(listen->getId(), listen1.getId());
            }

            {
                Listen::pointer listen{ Listen::find(session, user.getId(), track.getId(), ScrobblingBackend::Internal, dateTime2) };
                ASSERT_TRUE(listen);
                EXPECT_EQ(listen->getId(), listen2.getId());
            }

            {
                Listen::pointer listen{ Listen::find(session, user.getId(), track.getId(), ScrobblingBackend::Internal, dateTime2.addSecs(56)) };
                EXPECT_FALSE(listen);
            }
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopArtists)
    {
        ScopedTrack track1{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            const auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
            EXPECT_EQ(artists.moreResults, false);
        }

        ScopedTrack track2{ session };
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedListen listen2{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime1.addSecs(1) };

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track2.get(), artist1.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist1->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::ListenBrainz);

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 0);
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::ListenBrainz);
            params.setLinkType(TrackArtistLinkType::Producer);

            auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
        }

        {
            ScopedClusterType clusterType{ session, "MyType" };
            ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

            {
                auto transaction{ session.createReadTransaction() };

                Listen::ArtistStatsFindParameters params;
                params.setUser(user->getId());
                params.setScrobblingBackend(ScrobblingBackend::Internal);
                params.filters.setClusters(std::initializer_list<ClusterId>{ cluster->getId() });

                auto artists{ Listen::getTopArtists(session, params) };
                EXPECT_EQ(artists.results.size(), 0);
            }
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopArtists_multi)
    {
        ScopedUser user{ session, "MyUser" };
        ScopedTrack track1{ session };
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedTrack track2{ session };
        ScopedArtist artist2{ session, "MyArtist2" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track1.get(), artist1.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track2.get(), artist2.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
        }

        ScopedListen listen1{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist1->getId());
        }

        ScopedListen listen2{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(2) };
        ScopedListen listen3{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(3) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 2);
            EXPECT_EQ(artists.results[0], artist2->getId());
            EXPECT_EQ(artists.results[1], artist1->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.setRange(Range{ 0, 1 });

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.moreResults, true);
            EXPECT_EQ(artists.results[0], artist2->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopArtists_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            cluster.get().modify()->addTrack(track.get());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopArtists_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user.getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(library.getId());

            const auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
            EXPECT_EQ(artists.moreResults, false);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
            track.get().modify()->setMediaLibrary(library.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user.getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user.getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(library.getId());

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user.getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(otherLibrary.getId());

            auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopReleases)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedRelease release{ session, "MyRelease" };
        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            EXPECT_EQ(releases.results.size(), 0);
        }

        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0], release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::ListenBrainz);

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            EXPECT_EQ(releases.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopReleases_multi)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setRelease(release1.get());
            track2.get().modify()->setRelease(release2.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0], release1.getId());
        }
        ScopedListen listen2{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedListen listen3{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0], release2.getId());
            EXPECT_EQ(releases.results[1], release1.getId());
        }
        ScopedListen listen4{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedListen listen5{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0], release1.getId());
            EXPECT_EQ(releases.results[1], release2.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopReleases_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.results.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            cluster.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0], release.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopReleases_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
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

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(library.getId());

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            EXPECT_EQ(releases.results.size(), 0);
        }

        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(library.getId());

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0], release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(otherLibrary.getId());

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            EXPECT_EQ(releases.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopTracks)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 0);
        }

        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::ListenBrainz);

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            EXPECT_EQ(tracks.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopTracks_artist)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.setArtist(artist->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 0);
        }

        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.setArtist(artist->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.setArtist(artist->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopTrack_multi)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track1.getId());
        }
        ScopedListen listen2{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedListen listen3{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 2);
            EXPECT_EQ(tracks.results[0], track2.getId());
            EXPECT_EQ(tracks.results[1], track1.getId());
        }
        ScopedListen listen4{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedListen listen5{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 2);
            EXPECT_EQ(tracks.results[0], track1.getId());
            EXPECT_EQ(tracks.results[1], track2.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopTracks_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.results.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            cluster.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopTracks_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user.getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(library.getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setMediaLibrary(library.get());
        }

        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(library.getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setMediaLibrary(otherLibrary.getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            EXPECT_EQ(tracks.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentArtists)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
            EXPECT_EQ(artists.moreResults, false);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::ListenBrainz);

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 0);
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.setLinkType(TrackArtistLinkType::Producer);

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
        }
        {
            ScopedClusterType clusterType{ session, "MyType" };
            ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

            {
                auto transaction{ session.createReadTransaction() };

                Listen::ArtistStatsFindParameters params;
                params.setUser(user->getId());
                params.setScrobblingBackend(ScrobblingBackend::Internal);
                params.filters.setClusters(std::initializer_list<ClusterId>{ cluster->getId() });

                auto artists{ Listen::getRecentArtists(session, params) };
                EXPECT_EQ(artists.results.size(), 0);
            }
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentArtists_multi)
    {
        ScopedUser user{ session, "MyUser" };
        ScopedTrack track1{ session };
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedTrack track2{ session };
        ScopedArtist artist2{ session, "MyArtist2" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track1.get(), artist1.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track2.get(), artist2.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
        }

        ScopedListen listen1{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist1->getId());
        }
        ScopedListen listen2{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(2) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 2);
            EXPECT_EQ(artists.results[0], artist2->getId());
            EXPECT_EQ(artists.results[1], artist1->getId());
        }
        ScopedListen listen3{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(-1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.setRange(Range{ 0, 1 });

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.moreResults, true);
            EXPECT_EQ(artists.results[0], artist2->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentArtists_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            cluster.get().modify()->addTrack(track.get());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentArtists_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
            track.get().modify()->setMediaLibrary(library.get());
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.filters.setMediaLibrary(library.getId());

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results[0], artist->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.filters.setMediaLibrary(otherLibrary.getId());

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentReleases)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 0);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0], release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::ListenBrainz);

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            EXPECT_EQ(releases.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getMostRecentRelease)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto listen{ Listen::getMostRecentListen(session, user->getId(), ScrobblingBackend::Internal, release.getId()) };
            EXPECT_FALSE(listen);
        }

        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };

        {
            auto transaction{ session.createReadTransaction() };

            const auto listen{ Listen::getMostRecentListen(session, user->getId(), ScrobblingBackend::Internal, release.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime1);
        }

        const Wt::WDateTime dateTime2{ Wt::WDate{ 1999, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen2{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime2 };

        {
            auto transaction{ session.createReadTransaction() };

            const auto listen{ Listen::getMostRecentListen(session, user->getId(), ScrobblingBackend::Internal, release.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime1);
        }

        const Wt::WDateTime dateTime3{ Wt::WDate{ 2001, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen3{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime3 };

        {
            auto transaction{ session.createReadTransaction() };

            const auto listen{ Listen::getMostRecentListen(session, user->getId(), ScrobblingBackend::Internal, release.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime3);
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentReleases_multi)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setRelease(release1.get());
            track2.get().modify()->setRelease(release2.get());
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0], release2.getId());
        }

        ScopedListen listen2{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0], release1.getId());
            EXPECT_EQ(releases.results[1], release2.getId());
        }

        ScopedListen listen3{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(2) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0], release2.getId());
            EXPECT_EQ(releases.results[1], release1.getId());
        }

        ScopedListen listen4{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(-1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 2);
            EXPECT_EQ(releases.results[0], release2.getId());
            EXPECT_EQ(releases.results[1], release1.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentReleases_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.results.size(), 0);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            cluster.get().modify()->addTrack(track.get());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0], release.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentReleases_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release{ session, "MyRelease" };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setRelease(release.get());
            track.get().modify()->setMediaLibrary(library.get());
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.filters.setMediaLibrary(library.getId());

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results[0], release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.filters.setMediaLibrary(otherLibrary.getId());

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.moreResults, false);
            EXPECT_EQ(releases.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTracks)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 0);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::ListenBrainz);

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            EXPECT_EQ(tracks.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTracks_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setMediaLibrary(library.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.filters.setMediaLibrary(library.getId());

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.filters.setMediaLibrary(otherLibrary.getId());

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            EXPECT_EQ(tracks.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getCount_track)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };

            const std::size_t count{ Listen::getCount(session, user->getId(), track.getId()) };
            EXPECT_EQ(count, 0);
        }

        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };

        {
            auto transaction{ session.createReadTransaction() };

            const std::size_t count{ Listen::getCount(session, user->getId(), track.getId()) };
            EXPECT_EQ(count, 1);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            user.get().modify()->setScrobblingBackend(ScrobblingBackend::ListenBrainz);
        }

        {
            auto transaction{ session.createReadTransaction() };
            const std::size_t count{ Listen::getCount(session, user->getId(), track.getId()) };
            EXPECT_EQ(count, 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getCount_release)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release{ session, "MyRelease" };

        auto getReleaseListenCount{ [&] {
            auto transaction{ session.createReadTransaction() };
            return Listen::getCount(session, user->getId(), release.getId());
        } };

        EXPECT_EQ(getReleaseListenCount(), 0);

        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };

        EXPECT_EQ(getReleaseListenCount(), 0);

        {
            auto transaction{ session.createWriteTransaction() };
            track1.get().modify()->setRelease(release.get());
        }

        EXPECT_EQ(getReleaseListenCount(), 1);

        {
            auto transaction{ session.createWriteTransaction() };
            track2.get().modify()->setRelease(release.get());
        }

        EXPECT_EQ(getReleaseListenCount(), 0);
        ScopedListen listen2{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };
        EXPECT_EQ(getReleaseListenCount(), 1);
        ScopedListen listen3{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };
        EXPECT_EQ(getReleaseListenCount(), 1);

        ScopedListen listen4{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };
        EXPECT_EQ(getReleaseListenCount(), 2);

        {
            auto transaction{ session.createWriteTransaction() };
            user.get().modify()->setScrobblingBackend(ScrobblingBackend::ListenBrainz);
        }
        EXPECT_EQ(getReleaseListenCount(), 0);
    }

    TEST_F(DatabaseFixture, Listen_getMostRecentTrack)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };

            auto listen{ Listen::getMostRecentListen(session, user->getId(), ScrobblingBackend::Internal, track.getId()) };
            EXPECT_FALSE(listen);
        }

        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime1 };

        {
            auto transaction{ session.createReadTransaction() };

            auto listen{ Listen::getMostRecentListen(session, user->getId(), ScrobblingBackend::Internal, track.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime1);
        }

        const Wt::WDateTime dateTime2{ Wt::WDate{ 1999, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen2{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime2 };

        {
            auto transaction{ session.createReadTransaction() };

            auto listen{ Listen::getMostRecentListen(session, user->getId(), ScrobblingBackend::Internal, track.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime1);
        }

        const Wt::WDateTime dateTime3{ Wt::WDate{ 2001, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen3{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime3 };

        {
            auto transaction{ session.createReadTransaction() };

            auto listen{ Listen::getMostRecentListen(session, user->getId(), ScrobblingBackend::Internal, track.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime3);
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTracks_multi)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track2.getId());
        }

        ScopedListen listen2{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 2);
            EXPECT_EQ(tracks.results[0], track1.getId());
            EXPECT_EQ(tracks.results[1], track2.getId());
        }

        ScopedListen listen3{ session, user.lockAndGet(), track2.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(2) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 2);
            EXPECT_EQ(tracks.results[0], track2.getId());
            EXPECT_EQ(tracks.results[1], track1.getId());
        }

        ScopedListen listen4{ session, user.lockAndGet(), track1.lockAndGet(), ScrobblingBackend::Internal, dateTime.addSecs(-1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.moreResults, false);
            ASSERT_EQ(tracks.results.size(), 2);
            EXPECT_EQ(tracks.results[0], track2.getId());
            EXPECT_EQ(tracks.results[1], track1.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTracks_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.lockAndGet(), track.lockAndGet(), ScrobblingBackend::Internal, dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.results.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            cluster.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setScrobblingBackend(ScrobblingBackend::Internal);
            params.filters.setClusters(std::initializer_list<ClusterId>{ cluster.getId() });

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results[0], track.getId());
        }
    }
} // namespace lms::db::tests