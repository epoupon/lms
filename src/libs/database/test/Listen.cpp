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
#include "database/objects/ReleaseArtistLink.hpp"

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

        ScopedListen listen{ session, user.getId(), track.getId(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } } };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Listen::getCount(session), 1);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            ScopedListen listen2{ session, user.getId(), track.getId(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 13, 0, 1 } } };

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
        ScopedUser otherUser{ session, "OtherUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            auto listens{ Listen::find(session, Listen::FindParameters{}.setUser(otherUser->getId())) };
            EXPECT_EQ(listens.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto listens{ Listen::find(session, Listen::FindParameters{}.setUser(user->getId())) };
            ASSERT_EQ(listens.size(), 1);
            EXPECT_EQ(listens.front(), listen->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_get_multi)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedListen listen3{ session, user.getId(), track.getId(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 3 } } };
        ScopedListen listen1{ session, user.getId(), track.getId(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } } };
        ScopedListen listen2{ session, user.getId(), track.getId(), Wt::WDateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 2 } } };

        {
            auto transaction{ session.createReadTransaction() };

            auto listens{ Listen::find(session, Listen::FindParameters{}.setUser(user->getId())) };
            ASSERT_EQ(listens.size(), 3);
            EXPECT_EQ(listens[0], listen1.getId());
            EXPECT_EQ(listens[1], listen2.getId());
            EXPECT_EQ(listens[2], listen3.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_get_byDateTime)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        const Wt::WDateTime dateTime2{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 2 } };
        ASSERT_GT(dateTime2, dateTime1);

        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime1 };
        ScopedListen listen2{ session, user.getId(), track.getId(), dateTime2 };

        {
            auto transaction{ session.createReadTransaction() };

            {
                std::optional<Listen> listen{ Listen::find(session, user.getId(), track.getId(), dateTime1) };
                ASSERT_TRUE(listen);
                EXPECT_EQ(listen->getId(), listen1.getId());
            }

            {
                std::optional<Listen> listen{ Listen::find(session, user.getId(), track.getId(), dateTime2) };
                ASSERT_TRUE(listen);
                EXPECT_EQ(listen->getId(), listen2.getId());
            }

            {
                std::optional<Listen> listen{ Listen::find(session, user.getId(), track.getId(), dateTime2.addSecs(56)) };
                EXPECT_FALSE(listen);
            }
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopArtists)
    {
        ScopedTrack track1{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track1.getId(), dateTime1 };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            const auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        ScopedTrack track2{ session };
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedListen listen2{ session, user.getId(), track2.getId(), dateTime1.addSecs(1) };

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track2.getId(), artist1.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist1->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setTrackArtistLinkType(TrackArtistLinkType::Producer);

            auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        {
            ScopedClusterType clusterType{ session, "MyType" };
            ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

            {
                auto transaction{ session.createReadTransaction() };

                Listen::ArtistStatsFindParameters params;
                params.setUser(user->getId());
                params.setFilter(FilterExpr::equal(cluster->getId()));

                auto artists{ Listen::getTopArtists(session, params) };
                EXPECT_EQ(artists.size(), 0);
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
            TrackArtistLink::create(session, track1.getId(), artist1.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track2.getId(), artist2.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        ScopedListen listen1{ session, user.getId(), track1.getId(), dateTime.addSecs(1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist1->getId());
        }

        ScopedListen listen2{ session, user.getId(), track2.getId(), dateTime.addSecs(2) };
        ScopedListen listen3{ session, user.getId(), track2.getId(), dateTime.addSecs(3) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.size(), 2);
            EXPECT_EQ(artists[0], artist2->getId());
            EXPECT_EQ(artists[1], artist1->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setRange(Range{ 0, 1 });

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist2->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopArtists_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopArtists_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime1 };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user.getId());
            params.setFilter(FilterExpr::equal(library.getId()));

            const auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user.getId());

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user.getId());
            params.setFilter(FilterExpr::equal(library.getId()));

            auto artists{ Listen::getTopArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user.getId());
            params.setFilter(FilterExpr::equal(otherLibrary.getId()));

            auto artists{ Listen::getTopArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
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
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.size(), 0);
        }

        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getTopReleases(session, params) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopReleases_multi)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track1.getId(), dateTime };
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release2.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getTopReleases(session, params) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release1.getId());
        }
        ScopedListen listen2{ session, user.getId(), track2.getId(), dateTime };
        ScopedListen listen3{ session, user.getId(), track2.getId(), dateTime };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getTopReleases(session, params) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], release2.getId());
            EXPECT_EQ(releases[1], release1.getId());
        }
        ScopedListen listen4{ session, user.getId(), track1.getId(), dateTime };
        ScopedListen listen5{ session, user.getId(), track1.getId(), dateTime };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getTopReleases(session, params) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], release1.getId());
            EXPECT_EQ(releases[1], release2.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopReleases_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release.getId());
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
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            updatedTrack->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(library.getId()));

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.size(), 0);
        }

        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(library.getId()));

            auto releases{ Listen::getTopReleases(session, params) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(otherLibrary.getId()));

            auto releases{ Listen::getTopReleases(session, params) };
            EXPECT_EQ(releases.size(), 0);
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

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 0);
        }

        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track.getId());
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
            params.setArtist(artist->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 0);
        }

        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setArtist(artist->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setArtist(artist->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopTrack_multi)
    {
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track1.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track1.getId());
        }
        ScopedListen listen2{ session, user.getId(), track2.getId(), dateTime };
        ScopedListen listen3{ session, user.getId(), track2.getId(), dateTime };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 2);
            EXPECT_EQ(tracks[0], track2.getId());
            EXPECT_EQ(tracks[1], track1.getId());
        }
        ScopedListen listen4{ session, user.getId(), track1.getId(), dateTime };
        ScopedListen listen5{ session, user.getId(), track1.getId(), dateTime };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 2);
            EXPECT_EQ(tracks[0], track1.getId());
            EXPECT_EQ(tracks[1], track2.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopTracks_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track.getId());
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
            params.setFilter(FilterExpr::equal(library.getId()));

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack);
        }

        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(library.getId()));

            auto tracks{ Listen::getTopTracks(session, params) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(otherLibrary.getId()));

            auto tracks{ Listen::getTopTracks(session, params) };
            EXPECT_EQ(tracks.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentReleaseArtists)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
            ReleaseArtistLink::create(session, release.getId(), artist.getId(), false);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setReleaseArtistsOnly(true);

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setReleaseArtistsOnly(true);

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTrackArtists)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist->getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setReleaseArtistsOnly(true);

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setTrackArtistLinkType(TrackArtistLinkType::Producer);

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }
        {
            ScopedClusterType clusterType{ session, "MyType" };
            ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

            {
                auto transaction{ session.createReadTransaction() };

                Listen::ArtistStatsFindParameters params;
                params.setUser(user->getId());
                params.setFilter(FilterExpr::equal(cluster->getId()));

                auto artists{ Listen::getRecentArtists(session, params) };
                EXPECT_EQ(artists.size(), 0);
            }
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTrackArtists_multi)
    {
        ScopedUser user{ session, "MyUser" };
        ScopedTrack track1{ session };
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedTrack track2{ session };
        ScopedArtist artist2{ session, "MyArtist2" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track1.getId(), artist1.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track2.getId(), artist2.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }

        ScopedListen listen1{ session, user.getId(), track1.getId(), dateTime.addSecs(1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist1->getId());
        }
        ScopedListen listen2{ session, user.getId(), track2.getId(), dateTime.addSecs(2) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.size(), 2);
            EXPECT_EQ(artists[0], artist2->getId());
            EXPECT_EQ(artists[1], artist1->getId());
        }
        ScopedListen listen3{ session, user.getId(), track1.getId(), dateTime.addSecs(-1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setRange(Range{ 0, 1 });

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist2->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentArtists_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist.getId());
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
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setFilter(FilterExpr::equal(library.getId()));

            auto artists{ Listen::getRecentArtists(session, params) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists[0], artist->getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::ArtistStatsFindParameters params;
            params.setFilter(FilterExpr::equal(otherLibrary.getId()));

            auto artists{ Listen::getRecentArtists(session, params) };
            EXPECT_EQ(artists.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentReleases)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getRecentReleases(session, params) };
            ASSERT_EQ(releases.size(), 0);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getRecentReleases(session, params) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getMostRecentRelease)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto listen{ Listen::getMostRecentListen(session, user->getId(), release.getId()) };
            EXPECT_FALSE(listen);
        }

        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime1 };

        {
            auto transaction{ session.createReadTransaction() };

            const auto listen{ Listen::getMostRecentListen(session, user->getId(), release.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime1);
        }

        const Wt::WDateTime dateTime2{ Wt::WDate{ 1999, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen2{ session, user.getId(), track.getId(), dateTime2 };

        {
            auto transaction{ session.createReadTransaction() };

            const auto listen{ Listen::getMostRecentListen(session, user->getId(), release.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime1);
        }

        const Wt::WDateTime dateTime3{ Wt::WDate{ 2001, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen3{ session, user.getId(), track.getId(), dateTime3 };

        {
            auto transaction{ session.createReadTransaction() };

            const auto listen{ Listen::getMostRecentListen(session, user->getId(), release.getId()) };
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
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release1.getId());
            Track::update(session, *updatedTrack1);
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release2.getId());
            Track::update(session, *updatedTrack2);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track2.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getRecentReleases(session, params) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release2.getId());
        }

        ScopedListen listen2{ session, user.getId(), track1.getId(), dateTime.addSecs(1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getRecentReleases(session, params) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], release1.getId());
            EXPECT_EQ(releases[1], release2.getId());
        }

        ScopedListen listen3{ session, user.getId(), track2.getId(), dateTime.addSecs(2) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getRecentReleases(session, params) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], release2.getId());
            EXPECT_EQ(releases[1], release1.getId());
        }

        ScopedListen listen4{ session, user.getId(), track1.getId(), dateTime.addSecs(-1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto releases{ Listen::getRecentReleases(session, params) };
            ASSERT_EQ(releases.size(), 2);
            EXPECT_EQ(releases[0], release2.getId());
            EXPECT_EQ(releases[1], release1.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentReleases_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.size(), 0);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release.getId());
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
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            updatedTrack->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setFilter(FilterExpr::equal(library.getId()));

            auto releases{ Listen::getRecentReleases(session, params) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases[0], release.getId());
        }
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setFilter(FilterExpr::equal(otherLibrary.getId()));

            auto releases{ Listen::getRecentReleases(session, params) };
            EXPECT_EQ(releases.size(), 0);
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

            auto tracks{ Listen::getRecentTracks(session, params) };
            ASSERT_EQ(tracks.size(), 0);
        }

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getRecentTracks(session, params) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTracks_mediaLibrary)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };
        ScopedMediaLibrary otherLibrary{ session, "OtherLibrary", "/otherRoot" };

        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setMediaLibrary(library.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setFilter(FilterExpr::equal(library.getId()));

            auto tracks{ Listen::getRecentTracks(session, params) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setFilter(FilterExpr::equal(otherLibrary.getId()));

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.size(), 0);
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
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime1 };

        {
            auto transaction{ session.createReadTransaction() };

            const std::size_t count{ Listen::getCount(session, user->getId(), track.getId()) };
            EXPECT_EQ(count, 1);
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
        ScopedListen listen1{ session, user.getId(), track1.getId(), dateTime1 };

        EXPECT_EQ(getReleaseListenCount(), 0);

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setRelease(release.getId());
            Track::update(session, *updatedTrack1);
        }

        EXPECT_EQ(getReleaseListenCount(), 1);

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setRelease(release.getId());
            Track::update(session, *updatedTrack2);
        }

        EXPECT_EQ(getReleaseListenCount(), 0);
        ScopedListen listen2{ session, user.getId(), track2.getId(), dateTime1 };
        EXPECT_EQ(getReleaseListenCount(), 1);
        ScopedListen listen3{ session, user.getId(), track2.getId(), dateTime1 };
        EXPECT_EQ(getReleaseListenCount(), 1);

        ScopedListen listen4{ session, user.getId(), track1.getId(), dateTime1 };
        EXPECT_EQ(getReleaseListenCount(), 2);
    }

    TEST_F(DatabaseFixture, Listen_getMostRecentTrack)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };

            auto listen{ Listen::getMostRecentListen(session, user->getId(), track.getId()) };
            EXPECT_FALSE(listen);
        }

        const Wt::WDateTime dateTime1{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen1{ session, user.getId(), track.getId(), dateTime1 };

        {
            auto transaction{ session.createReadTransaction() };

            auto listen{ Listen::getMostRecentListen(session, user->getId(), track.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime1);
        }

        const Wt::WDateTime dateTime2{ Wt::WDate{ 1999, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen2{ session, user.getId(), track.getId(), dateTime2 };

        {
            auto transaction{ session.createReadTransaction() };

            auto listen{ Listen::getMostRecentListen(session, user->getId(), track.getId()) };
            EXPECT_TRUE(listen);
            EXPECT_EQ(listen->getDateTime(), dateTime1);
        }

        const Wt::WDateTime dateTime3{ Wt::WDate{ 2001, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen3{ session, user.getId(), track.getId(), dateTime3 };

        {
            auto transaction{ session.createReadTransaction() };

            auto listen{ Listen::getMostRecentListen(session, user->getId(), track.getId()) };
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
        ScopedListen listen1{ session, user.getId(), track2.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getRecentTracks(session, params) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track2.getId());
        }

        ScopedListen listen2{ session, user.getId(), track1.getId(), dateTime.addSecs(1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getRecentTracks(session, params) };
            ASSERT_EQ(tracks.size(), 2);
            EXPECT_EQ(tracks[0], track1.getId());
            EXPECT_EQ(tracks[1], track2.getId());
        }

        ScopedListen listen3{ session, user.getId(), track2.getId(), dateTime.addSecs(2) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getRecentTracks(session, params) };
            ASSERT_EQ(tracks.size(), 2);
            EXPECT_EQ(tracks[0], track2.getId());
            EXPECT_EQ(tracks[1], track1.getId());
        }

        ScopedListen listen4{ session, user.getId(), track1.getId(), dateTime.addSecs(-1) };
        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());

            auto tracks{ Listen::getRecentTracks(session, params) };
            ASSERT_EQ(tracks.size(), 2);
            EXPECT_EQ(tracks[0], track2.getId());
            EXPECT_EQ(tracks[1], track1.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTracks_cluster)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.size(), 0);
        }
        {
            auto transaction{ session.createWriteTransaction() };
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            params.setFilter(FilterExpr::equal(cluster.getId()));

            auto tracks{ Listen::getRecentTracks(session, params) };
            EXPECT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks[0], track.getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopArtists_callback)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ArtistId> ids;
            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            Listen::getTopArtists(session, params, [&](const Artist& a) {
                ids.push_back(a.getId());
            });
            ASSERT_EQ(ids.size(), 1);
            EXPECT_EQ(ids[0], artist->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopReleases_callback)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release{ session, "MyRelease" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ReleaseId> ids;
            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            Listen::getTopReleases(session, params, [&](const Release& r) {
                ids.push_back(r.getId());
            });
            ASSERT_EQ(ids.size(), 1);
            EXPECT_EQ(ids[0], release->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getTopTracks_callback)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackId> ids;
            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            Listen::getTopTracks(session, params, [&](const Track& t) {
                ids.push_back(t.getId());
            });
            ASSERT_EQ(ids.size(), 1);
            EXPECT_EQ(ids[0], track->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentArtists_callback)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedArtist artist{ session, "MyArtist" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ArtistId> ids;
            Listen::ArtistStatsFindParameters params;
            params.setUser(user->getId());
            Listen::getRecentArtists(session, params, [&](const Artist& a) {
                ids.push_back(a.getId());
            });
            ASSERT_EQ(ids.size(), 1);
            EXPECT_EQ(ids[0], artist->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentReleases_callback)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        ScopedRelease release{ session, "MyRelease" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ReleaseId> ids;
            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            Listen::getRecentReleases(session, params, [&](const Release& r) {
                ids.push_back(r.getId());
            });
            ASSERT_EQ(ids.size(), 1);
            EXPECT_EQ(ids[0], release->getId());
        }
    }

    TEST_F(DatabaseFixture, Listen_getRecentTracks_callback)
    {
        ScopedTrack track{ session };
        ScopedUser user{ session, "MyUser" };
        const Wt::WDateTime dateTime{ Wt::WDate{ 2000, 1, 2 }, Wt::WTime{ 12, 0, 1 } };
        ScopedListen listen{ session, user.getId(), track.getId(), dateTime };

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<TrackId> ids;
            Listen::StatsFindParameters params;
            params.setUser(user->getId());
            Listen::getRecentTracks(session, params, [&](const Track& t) {
                ids.push_back(t.getId());
            });
            ASSERT_EQ(ids.size(), 1);
            EXPECT_EQ(ids[0], track->getId());
        }
    }
} // namespace lms::db::tests