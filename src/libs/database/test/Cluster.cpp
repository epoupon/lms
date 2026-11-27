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

#include <algorithm>
#include <list>

namespace lms::db::tests
{
    TEST_F(DatabaseFixture, Cluster)
    {
        {
            auto transaction{ session.createWriteTransaction() };
            EXPECT_EQ(Cluster::getCount(session), 0);
            EXPECT_EQ(ClusterType::getCount(session), 0);
        }

        ScopedClusterType clusterType{ session, "MyType" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(ClusterType::getCount(session), 1);
        }

        {
            ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

            {
                auto transaction{ session.createReadTransaction() };

                EXPECT_EQ(Cluster::getCount(session), 1);
                EXPECT_EQ(cluster->getTypeId(), clusterType.getId());

                {
                    const auto clusters{ Cluster::findIds(session, Cluster::FindParameters{}) };
                    ASSERT_EQ(clusters.size(), 1);
                    EXPECT_EQ(clusters.front(), cluster.getId());
                }

                {
                    const auto clusters{ Cluster::findOrphanIds(session) };
                    ASSERT_EQ(clusters.size(), 1);
                    EXPECT_EQ(clusters.front(), cluster.getId());
                }

                auto clusterTypes{ ClusterType::findIds(session) };
                ASSERT_EQ(clusterTypes.size(), 1);
                EXPECT_EQ(clusterTypes.front(), clusterType.getId());

                clusterTypes = ClusterType::findUsed(session);
                ASSERT_EQ(clusterTypes.size(), 1);
                EXPECT_EQ(clusterTypes.front(), clusterType.getId());

                clusterTypes = ClusterType::findOrphanIds(session);
                EXPECT_EQ(clusterTypes.size(), 0);
            }
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto clusterTypes{ ClusterType::findOrphanIds(session) };
            ASSERT_EQ(clusterTypes.size(), 1);
            EXPECT_EQ(clusterTypes.front(), clusterType.getId());

            ASSERT_EQ(ClusterType::findUsed(session).size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Cluster_find)
    {
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster1{ session, clusterType.getId(), "MyCluster" };
        ScopedCluster cluster2{ session, clusterType.getId(), "Mycluster" };
        ScopedCluster cluster3{ session, clusterType.getId(), "MyOtherCluster" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Cluster::find(session, clusterType.getId(), "MyCluster")->getId(), cluster1.getId());
            EXPECT_EQ(Cluster::find(session, clusterType.getId(), "Mycluster")->getId(), cluster2.getId());

            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), " Mycluster"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), "Mycluster "));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), "mycluster"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), "My"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), "Cluster"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), "MyCluster1"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), "MyCluster2"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), ""));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), " "));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), "*"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), R"(%)"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), R"(%%)"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), R"(")"));
            EXPECT_FALSE(Cluster::find(session, clusterType.getId(), R"("")"));
        }
    }

    TEST_F(DatabaseFixture, Cluster_create)
    {
        ScopedClusterType clusterType{ session, "MyType" };

        {
            auto transaction{ session.createWriteTransaction() };

            const Cluster createdCluster{ Cluster::create(session, clusterType.getId(), "Foo") };
            const std::optional<Cluster> foundCluster{ Cluster::find(session, clusterType.getId(), "Foo") };
            ASSERT_TRUE(foundCluster);
            EXPECT_EQ(createdCluster.getId(), foundCluster->getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            const Cluster createdCluster{ Cluster::create(session, clusterType.getId(), "") };
            const std::optional<Cluster> foundCluster{ Cluster::find(session, clusterType.getId(), "") };
            ASSERT_TRUE(foundCluster);
            EXPECT_EQ(createdCluster.getId(), foundCluster->getId());
        }
    }

    TEST_F(DatabaseFixture, Cluster_create_long)
    {
        ScopedClusterType clusterType{ session, "MyType" };

        {
            auto transaction{ session.createWriteTransaction() };

            const Cluster createdCluster{ Cluster::create(session, clusterType.getId(), "Alternative Rock; Art Pop; Art Rock; Britpop; Chamber Pop; Electronic Rock; Electronica; Experimental Rock; Neo-Progressive Rock; Foo") };
            const std::optional<Cluster> foundCluster{ Cluster::find(session, clusterType.getId(), "Alternative Rock; Art Pop; Art Rock; Britpop; Chamber Pop; Electronic Rock; Electronica; Experimental Rock; Neo-Progressive Rock; Foo") };
            ASSERT_TRUE(foundCluster);
            EXPECT_EQ(createdCluster.getId(), foundCluster->getId());
        }
    }

    TEST_F(DatabaseFixture, Cluster_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedClusterType clusterType{ session, "MyClusterType" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).size(), 0);
            auto clusterTypes{ ClusterType::findOrphanIds(session) };
            ASSERT_EQ(clusterTypes.size(), 1);
            EXPECT_EQ(clusterTypes.front(), clusterType.getId());
        }

        ScopedCluster cluster1{ session, clusterType.getId(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.getId(), "MyCluster2" };

        {
            auto transaction{ session.createReadTransaction() };
            auto clusters{ Cluster::findOrphanIds(session) };
            EXPECT_EQ(clusters.size(), 2);
            EXPECT_EQ(Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId())).size(), 0);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster1.getId()), 0);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster2.getId()), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            Cluster::addTrack(session, cluster1.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto clusters{ Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId())) };
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front(), cluster1.getId());
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster1.getId()), 1);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster2.getId()), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto clusters{ Cluster::findOrphanIds(session) };
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front(), cluster2.getId());

            EXPECT_EQ(ClusterType::findOrphanIds(session).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilter(FilterExpr::equal(cluster1.getId()))) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());

            tracks = Track::findIds(session, Track::FindParameters{}.setFilter(FilterExpr::equal(cluster2.getId())));
            EXPECT_EQ(tracks.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto clusterIds{ Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId())) };
            ASSERT_EQ(clusterIds.size(), 1);
            EXPECT_EQ(clusterIds.front(), cluster1.getId());
        }
    }

    TEST_F(DatabaseFixture, Cluster_singleTrackWithSeveralClusters)
    {
        ScopedTrack track{ session };
        ScopedClusterType clusterType{ session, "MyClusterType" };

        ScopedCluster cluster1{ session, clusterType.getId(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.getId(), "MyCluster2" };

        const FilterExpr clusterFilter{ FilterExpr::And({ FilterExpr::equal(cluster1.getId()), FilterExpr::equal(cluster2.getId()) }) };

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilter(clusterFilter)) };
            EXPECT_EQ(tracks.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            Cluster::addTrack(session, cluster1.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilter(clusterFilter)) };
            EXPECT_EQ(tracks.size(), 0);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster1.getId()), 1);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster2.getId()), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            Cluster::addTrack(session, cluster2.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilter(clusterFilter)) };
            ASSERT_EQ(tracks.size(), 1);
            EXPECT_EQ(tracks.front(), track.getId());
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster1.getId()), 1);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster2.getId()), 1);
        }
    }

    TEST_F(DatabaseFixture, Cluster_multiTracks)
    {
        std::list<ScopedTrack> tracks;
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

        for (std::size_t i{}; i < 10; ++i)
        {
            tracks.emplace_back(session);

            {
                auto transaction{ session.createWriteTransaction() };
                Cluster::addTrack(session, cluster.getId(), tracks.back().getId());
            }
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).size(), 0);

            EXPECT_EQ(Cluster::computeTrackCount(session, cluster.getId()), tracks.size());

            for (TrackId trackId : Track::findIds(session, Track::FindParameters{}.setFilter(FilterExpr::equal(cluster.getId()))))
            {
                auto it{ std::find_if(std::cbegin(tracks), std::cend(tracks), [&](const ScopedTrack& track) { return trackId == track.getId(); }) };
                EXPECT_TRUE(it != std::cend(tracks));
            }
        }
    }

    TEST_F(DatabaseFixture, ClusterType)
    {
        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            ClusterType::find(session, [&](const ClusterType&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        ScopedClusterType clusterType1{ session, "MyClusterType1" };
        ScopedClusterType clusterType2{ session, "MyClusterType2" };

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ClusterTypeId> visitedClusterTypes;
            ClusterType::find(session, [&](const ClusterType& clusterType) {
                visitedClusterTypes.push_back(clusterType.getId());
            });
            ASSERT_EQ(visitedClusterTypes.size(), 2);
            EXPECT_EQ(visitedClusterTypes[0], clusterType1->getId());
            EXPECT_EQ(visitedClusterTypes[1], clusterType2->getId());
        }
    }

    TEST_F(DatabaseFixture, ClusterType_singleTrack)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findIds(session, Cluster::FindParameters{}).size(), 0);
            EXPECT_EQ(Cluster::findIds(session, Cluster::FindParameters{}.setClusterTypeName("Foo")).size(), 0);
        }

        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

        {
            auto transaction{ session.createReadTransaction() };
            auto clusters{ Cluster::findIds(session, Cluster::FindParameters{}) };
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front(), cluster.getId());

            clusters = Cluster::findIds(session, Cluster::FindParameters{}.setClusterType(clusterType.getId()));
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front(), cluster.getId());

            clusters = Cluster::findIds(session, Cluster::FindParameters{}.setClusterTypeName("Foo"));
            EXPECT_EQ(clusters.size(), 0);

            clusters = Cluster::findIds(session, Cluster::FindParameters{}.setClusterTypeName("MyClusterType"));
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front(), cluster.getId());
        }
    }

    TEST_F(DatabaseFixture, Cluster_singleTrackSingleReleaseSingleCluster)
    {
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).size(), 0);
        }

        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };
        ScopedCluster unusedCluster{ session, clusterType.getId(), "MyClusterUnused" };

        {
            auto transaction{ session.createReadTransaction() };
            ASSERT_EQ(Cluster::findOrphanIds(session).size(), 2);
            EXPECT_EQ(Release::findIds(session, Release::FindParameters{}.setFilter(FilterExpr::equal(unusedCluster.getId()))).size(), 0);
            EXPECT_EQ(Release::findIds(session, Release::FindParameters{}).size(), 1);
            EXPECT_EQ(Cluster::computeReleaseCount(session, cluster.getId()), 0);
            EXPECT_EQ(Cluster::computeReleaseCount(session, unusedCluster.getId()), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                auto clusters{ Cluster::findOrphanIds(session) };
                ASSERT_EQ(clusters.size(), 1);
                EXPECT_EQ(clusters.front(), unusedCluster.getId());
            }
            EXPECT_EQ(Cluster::computeReleaseCount(session, cluster.getId()), 1);
            EXPECT_EQ(Cluster::computeReleaseCount(session, unusedCluster.getId()), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto clusters{ Cluster::findIds(session, Cluster::FindParameters{}.setRelease(release.getId())) };
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front(), cluster.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(FilterExpr::equal(cluster.getId()))) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setFilter(FilterExpr::equal(unusedCluster.getId()))) };
            EXPECT_EQ(releases.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Cluster::computeReleaseCount(session, cluster.getId()), 1);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster.getId()), 1);
            EXPECT_EQ(Cluster::computeReleaseCount(session, unusedCluster.getId()), 0);
            EXPECT_EQ(Cluster::computeTrackCount(session, unusedCluster.getId()), 0);
        }
    }

    TEST_F(DatabaseFixture, SingleTrackSingleArtistMultiClusters)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster1{ session, clusterType.getId(), "Cluster1" };
        ScopedCluster cluster2{ session, clusterType.getId(), "Cluster2" };
        ScopedCluster cluster3{ session, clusterType.getId(), "Cluster3" };
        {
            auto transaction{ session.createWriteTransaction() };

            auto trackArtistLink{ TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist) };
            Cluster::addTrack(session, cluster1.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(ClusterType::findOrphanIds(session).size(), 0);
            EXPECT_EQ(Cluster::findOrphanIds(session).size(), 2);
            EXPECT_EQ(Release::findOrphanIds(session).size(), 0);
            EXPECT_EQ(Artist::findOrphanIds(session).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId())).size(), 1);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster1.getId()))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());

            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster2.getId()))).size(), 0);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster3.getId()))).size(), 0);

            Cluster::addTrack(session, cluster2.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster1.getId()))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());

            artists = Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster2.getId())));
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());

            artists = Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster1.getId())));
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());

            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster3.getId()))).size(), 0);
        }
    }

    TEST_F(DatabaseFixture, SingleTrackSingleArtistMultiRolesMultiClusters)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Mixer);
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).size(), 0);
            EXPECT_EQ(Release::findOrphanIds(session).size(), 0);
            EXPECT_EQ(Artist::findOrphanIds(session).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster.getId()))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }
    }

    TEST_F(DatabaseFixture, MultiTracksSingleArtistMultiClusters)
    {
        constexpr std::size_t nbTracks{ 10 };
        constexpr std::size_t nbClusters{ 5 };

        std::list<ScopedTrack> tracks;
        std::list<ScopedCluster> clusters;
        ScopedArtist artist{ session, "MyArtist" };
        ScopedClusterType clusterType{ session, "MyType" };

        for (std::size_t i{}; i < nbClusters; ++i)
            clusters.emplace_back(session, clusterType.getId(), "MyCluster" + std::to_string(i));

        for (std::size_t i{}; i < nbTracks; ++i)
        {
            tracks.emplace_back(session);

            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, tracks.back().getId(), artist.getId(), TrackArtistLinkType::Artist);

            for (auto& cluster : clusters)
                Cluster::addTrack(session, cluster.getId(), tracks.back().getId());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).size(), 0);
            EXPECT_EQ(Artist::findOrphanIds(session).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<FilterExpr> clusterConditions;
            std::transform(std::cbegin(clusters), std::cend(clusters), std::back_inserter(clusterConditions), [](const ScopedCluster& cluster) { return FilterExpr::equal(cluster.getId()); });

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::And(clusterConditions))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());
        }
    }

    TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtistSingleCluster)
    {
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.getId(), "MyCluster" };

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist);
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
            Cluster::addTrack(session, cluster.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Cluster::findOrphanIds(session).size(), 0);
            EXPECT_EQ(ClusterType::findOrphanIds(session).size(), 0);
            EXPECT_EQ(Artist::findOrphanIds(session).size(), 0);
            EXPECT_EQ(Release::findOrphanIds(session).size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilter(FilterExpr::equal(cluster.getId()))) };
            ASSERT_EQ(artists.size(), 1);
            EXPECT_EQ(artists.front(), artist.getId());

            auto releases{ Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId())) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId()).setFilter(FilterExpr::equal(cluster.getId())));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
        }
    }

    TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtistMultiClusters)
    {
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster1{ session, clusterType.getId(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.getId(), "MyCluster2" };

        {
            auto transaction{ session.createWriteTransaction() };

            auto trackArtistLink{ TrackArtistLink::create(session, track.getId(), artist.getId(), TrackArtistLinkType::Artist) };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setRelease(release.getId());
            Track::update(session, *updatedTrack);
            Cluster::addTrack(session, cluster1.getId(), track.getId());
            Cluster::addTrack(session, cluster2.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId())) };
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setTrackArtist(artist.getId()).setFilter(FilterExpr::And({ FilterExpr::equal(cluster1.getId()), FilterExpr::equal(cluster2.getId()) })));
            ASSERT_EQ(releases.size(), 1);
            EXPECT_EQ(releases.front(), release.getId());
        }
    }

    TEST_F(DatabaseFixture, Cluster_findByTrackAndTypeName)
    {
        ScopedTrack track{ session };
        ScopedClusterType genreType{ session, "GENRE" };
        ScopedClusterType moodType{ session, "MOOD" };
        ScopedCluster genre1{ session, genreType.getId(), "Rock" };
        ScopedCluster genre2{ session, genreType.getId(), "Pop" };
        ScopedCluster mood{ session, moodType.getId(), "Happy" };

        {
            auto transaction{ session.createWriteTransaction() };
            Cluster::addTrack(session, genre1.getId(), track.getId());
            Cluster::addTrack(session, genre2.getId(), track.getId());
            Cluster::addTrack(session, mood.getId(), track.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto genres{ Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId()).setClusterTypeName("GENRE")) };
            ASSERT_EQ(genres.size(), 2);

            const auto moods{ Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId()).setClusterTypeName("MOOD")) };
            ASSERT_EQ(moods.size(), 1);
            EXPECT_EQ(moods.front(), mood.getId());

            const auto all{ Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId())) };
            EXPECT_EQ(all.size(), 3);

            const auto none{ Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId()).setClusterTypeName("GROUPING")) };
            EXPECT_EQ(none.size(), 0);
        }
    }

    namespace
    {
        std::vector<ClusterId> findClusterIds(ISession& session, const Cluster::FindParameters& params)
        {
            auto transaction{ session.createReadTransaction() };
            return Cluster::findIds(session, params);
        }

        // For the clusters that are equally frequent
        std::vector<ClusterId> sorted(std::vector<ClusterId> ids)
        {
            std::sort(std::begin(ids), std::end(ids));
            return ids;
        }
    } // namespace

    TEST_F(DatabaseFixture, Cluster_find_byTrack)
    {
        ScopedClusterType typeA{ session, "TypeA" };
        ScopedClusterType typeB{ session, "TypeB" };
        ScopedCluster a1{ session, typeA.getId(), "A1" };
        ScopedCluster a2{ session, typeA.getId(), "A2" };
        ScopedCluster a3{ session, typeA.getId(), "A3" };
        ScopedCluster a4{ session, typeA.getId(), "A4" };
        ScopedCluster b1{ session, typeB.getId(), "B1" };
        ScopedCluster b2{ session, typeB.getId(), "B2" };
        ScopedTrack track{ session };
        ScopedTrack otherTrack{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            for (ScopedCluster* cluster : { &a1, &a2, &a3, &b1 })
                Cluster::addTrack(session, cluster->getId(), track.getId());
            for (ScopedCluster* cluster : { &a4, &b2 })
                Cluster::addTrack(session, cluster->getId(), otherTrack.getId());
        }

        const Cluster::FindParameters params{ Cluster::FindParameters{}.setTrack(track.getId()).setSortMethod(ClusterSortMethod::Name) };
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setClusterType(typeA.getId())), (std::vector{ a1.getId(), a2.getId(), a3.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setClusterType(typeA.getId()).setRange(Range{ 0, 2 })), (std::vector{ a1.getId(), a2.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setClusterType(typeB.getId())), (std::vector{ b1.getId() }));
        EXPECT_EQ(findClusterIds(session, params), (std::vector{ a1.getId(), a2.getId(), a3.getId(), b1.getId() }));
    }

    TEST_F(DatabaseFixture, Cluster_find_byRelease_mostFrequentFirst)
    {
        ScopedClusterType typeA{ session, "TypeA" };
        ScopedClusterType typeB{ session, "TypeB" };
        // created from the most to the least frequent: the order of the result is not the order of the ids
        ScopedCluster popular{ session, typeA.getId(), "Popular" };
        ScopedCluster common{ session, typeA.getId(), "Common" };
        ScopedCluster rare{ session, typeA.getId(), "Rare" };
        ScopedCluster otherReleaseOnly{ session, typeA.getId(), "OtherReleaseOnly" };
        ScopedCluster b1{ session, typeB.getId(), "B1" };
        ScopedRelease release{ session, "MyRelease" };
        ScopedRelease otherRelease{ session, "MyOtherRelease" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedTrack otherTrack{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            for (ScopedTrack* track : { &track1, &track2, &track3 })
            {
                std::optional<Track> updatedTrack{ track->get() };
                updatedTrack->setRelease(release.getId());
                Track::update(session, *updatedTrack);
            }
            std::optional<Track> updatedOtherTrack{ otherTrack.get() };
            updatedOtherTrack->setRelease(otherRelease.getId());
            Track::update(session, *updatedOtherTrack);

            Cluster::addTrack(session, popular.getId(), track1.getId());
            Cluster::addTrack(session, popular.getId(), track2.getId());
            Cluster::addTrack(session, popular.getId(), track3.getId());
            Cluster::addTrack(session, common.getId(), track1.getId());
            Cluster::addTrack(session, common.getId(), track2.getId());
            Cluster::addTrack(session, rare.getId(), track1.getId());
            Cluster::addTrack(session, b1.getId(), track2.getId());

            // the tracks of the other release are not counted
            Cluster::addTrack(session, otherReleaseOnly.getId(), otherTrack.getId());
            Cluster::addTrack(session, popular.getId(), otherTrack.getId());
        }

        const Cluster::FindParameters params{ Cluster::FindParameters{}.setSortMethod(ClusterSortMethod::TrackCountDesc) };
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setRelease(release.getId()).setClusterType(typeA.getId())), (std::vector{ popular.getId(), common.getId(), rare.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setRelease(release.getId()).setClusterType(typeA.getId()).setRange(Range{ 0, 2 })), (std::vector{ popular.getId(), common.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setRelease(release.getId()).setClusterType(typeB.getId())), (std::vector{ b1.getId() }));
        EXPECT_EQ(sorted(findClusterIds(session, Cluster::FindParameters{ params }.setRelease(otherRelease.getId()).setClusterType(typeA.getId()))), sorted({ popular.getId(), otherReleaseOnly.getId() }));
    }

    TEST_F(DatabaseFixture, Cluster_find_byArtist_mostFrequentFirst)
    {
        ScopedClusterType typeA{ session, "TypeA" };
        ScopedClusterType typeB{ session, "TypeB" };
        ScopedCluster twoTracks{ session, typeA.getId(), "TwoTracks" };
        ScopedCluster oneTrack{ session, typeA.getId(), "OneTrack" };
        ScopedCluster otherArtistOnly{ session, typeA.getId(), "OtherArtistOnly" };
        ScopedCluster b1{ session, typeB.getId(), "B1" };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedArtist otherArtist{ session, "MyOtherArtist" };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedTrack otherTrack{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            // linked three times to the same track: it still is a single track, counted per link it would come first
            TrackArtistLink::create(session, track1.getId(), artist.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track1.getId(), artist.getId(), TrackArtistLinkType::Composer);
            TrackArtistLink::create(session, track1.getId(), artist.getId(), TrackArtistLinkType::Lyricist);
            TrackArtistLink::create(session, track2.getId(), artist.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track3.getId(), artist.getId(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, otherTrack.getId(), otherArtist.getId(), TrackArtistLinkType::Artist);

            Cluster::addTrack(session, oneTrack.getId(), track1.getId());
            Cluster::addTrack(session, twoTracks.getId(), track2.getId());
            Cluster::addTrack(session, twoTracks.getId(), track3.getId());
            Cluster::addTrack(session, b1.getId(), track1.getId());
            Cluster::addTrack(session, otherArtistOnly.getId(), otherTrack.getId());
        }

        const Cluster::FindParameters params{ Cluster::FindParameters{}.setSortMethod(ClusterSortMethod::TrackCountDesc) };
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setArtist(artist.getId()).setClusterType(typeA.getId())), (std::vector{ twoTracks.getId(), oneTrack.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setArtist(artist.getId()).setClusterType(typeA.getId()).setRange(Range{ 0, 1 })), (std::vector{ twoTracks.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setArtist(artist.getId()).setClusterType(typeB.getId())), (std::vector{ b1.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setArtist(otherArtist.getId()).setClusterType(typeA.getId())), (std::vector{ otherArtistOnly.getId() }));
    }

    TEST_F(DatabaseFixture, Cluster_find_byTrackList_mostFrequentFirst)
    {
        ScopedClusterType typeA{ session, "TypeA" };
        ScopedClusterType typeB{ session, "TypeB" };
        ScopedCluster onTwoTracks{ session, typeA.getId(), "OnTwoTracks" };
        ScopedCluster onListedThrice{ session, typeA.getId(), "OnListedThrice" };
        ScopedCluster notInTrackList{ session, typeA.getId(), "NotInTrackList" };
        ScopedCluster b1{ session, typeB.getId(), "B1" };
        ScopedTrackList trackList{ session, "MyTrackList", TrackListType::PlayList };
        ScopedTrackList otherTrackList{ session, "MyOtherTrackList", TrackListType::PlayList };
        ScopedTrack track1{ session };
        ScopedTrack track2{ session };
        ScopedTrack track3{ session };
        ScopedTrack otherTrack{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            // a track listed three times is still a single track, counted per entry it would come first
            for (int i{}; i < 3; ++i)
                TrackListEntry::create(session, track1.getId(), trackList.getId());
            TrackListEntry::create(session, track2.getId(), trackList.getId());
            TrackListEntry::create(session, track3.getId(), trackList.getId());
            TrackListEntry::create(session, otherTrack.getId(), otherTrackList.getId());

            Cluster::addTrack(session, onTwoTracks.getId(), track2.getId());
            Cluster::addTrack(session, onTwoTracks.getId(), track3.getId());
            Cluster::addTrack(session, onListedThrice.getId(), track1.getId());
            Cluster::addTrack(session, b1.getId(), track2.getId());
            Cluster::addTrack(session, notInTrackList.getId(), otherTrack.getId());
        }

        const Cluster::FindParameters params{ Cluster::FindParameters{}.setSortMethod(ClusterSortMethod::TrackCountDesc) };
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setTrackList(trackList.getId()).setClusterType(typeA.getId())), (std::vector{ onTwoTracks.getId(), onListedThrice.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setTrackList(trackList.getId()).setClusterType(typeA.getId()).setRange(Range{ 0, 1 })), (std::vector{ onTwoTracks.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setTrackList(trackList.getId()).setClusterType(typeB.getId())), (std::vector{ b1.getId() }));
        EXPECT_EQ(findClusterIds(session, Cluster::FindParameters{ params }.setTrackList(otherTrackList.getId()).setClusterType(typeA.getId())), (std::vector{ notInTrackList.getId() }));
    }

} // namespace lms::db::tests