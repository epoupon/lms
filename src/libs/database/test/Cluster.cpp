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
            ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

            {
                auto transaction{ session.createReadTransaction() };

                EXPECT_EQ(Cluster::getCount(session), 1);
                EXPECT_EQ(cluster->getType()->getId(), clusterType.getId());

                {
                    const auto clusters{ Cluster::findIds(session, Cluster::FindParameters{}) };
                    ASSERT_EQ(clusters.results.size(), 1);
                    EXPECT_EQ(clusters.results.front(), cluster.getId());
                }

                {
                    const auto clusters{ Cluster::findOrphanIds(session) };
                    ASSERT_EQ(clusters.results.size(), 1);
                    EXPECT_EQ(clusters.results.front(), cluster.getId());
                }

                auto clusterTypes{ ClusterType::findIds(session) };
                ASSERT_EQ(clusterTypes.results.size(), 1);
                EXPECT_EQ(clusterTypes.results.front(), clusterType.getId());

                clusterTypes = ClusterType::findUsed(session);
                ASSERT_EQ(clusterTypes.results.size(), 1);
                EXPECT_EQ(clusterTypes.results.front(), clusterType.getId());

                clusterTypes = ClusterType::findOrphanIds(session);
                EXPECT_EQ(clusterTypes.results.size(), 0);
            }
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto clusterTypes{ ClusterType::findOrphanIds(session) };
            ASSERT_EQ(clusterTypes.results.size(), 1);
            EXPECT_EQ(clusterTypes.results.front(), clusterType.getId());

            ASSERT_EQ(ClusterType::findUsed(session).results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Cluster_find)
    {
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "Mycluster" };
        ScopedCluster cluster3{ session, clusterType.lockAndGet(), "MyOtherCluster" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(clusterType->getCluster("MyCluster"), cluster1.get());
            EXPECT_EQ(clusterType->getCluster("Mycluster"), cluster2.get());

            EXPECT_EQ(clusterType->getCluster(" Mycluster"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster("Mycluster "), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster("mycluster"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster("My"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster("Cluster"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster("MyCluster1"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster("MyCluster2"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster(""), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster(" "), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster("*"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster(R"(%)"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster(R"(%%)"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster(R"(")"), Cluster::pointer{});
            EXPECT_EQ(clusterType->getCluster(R"("")"), Cluster::pointer{});
        }
    }

    TEST_F(DatabaseFixture, Cluster_create)
    {
        ScopedClusterType clusterType{ session, "MyType" };

        {
            auto transaction{ session.createWriteTransaction() };

            auto createdCluster{ session.create<Cluster>(clusterType.get(), "Foo") };
            auto foundCluster{ clusterType->getCluster("Foo") };
            EXPECT_EQ(createdCluster, foundCluster);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            auto createdCluster{ session.create<Cluster>(clusterType.get(), "") };
            auto foundCluster{ clusterType->getCluster("") };
            EXPECT_EQ(createdCluster, foundCluster);
        }
    }

    TEST_F(DatabaseFixture, Cluster_create_long)
    {
        ScopedClusterType clusterType{ session, "MyType" };

        {
            auto transaction{ session.createWriteTransaction() };

            auto createdCluster{ session.create<Cluster>(clusterType.get(), "Alternative Rock; Art Pop; Art Rock; Britpop; Chamber Pop; Electronic Rock; Electronica; Experimental Rock; Neo-Progressive Rock; Foo") };
            auto foundCluster{ clusterType->getCluster("Alternative Rock; Art Pop; Art Rock; Britpop; Chamber Pop; Electronic Rock; Electronica; Experimental Rock; Neo-Progressive Rock; Foo") };
            EXPECT_EQ(createdCluster, foundCluster);
        }
    }

    TEST_F(DatabaseFixture, Cluster_singleTrack)
    {
        ScopedTrack track{ session };
        ScopedClusterType clusterType{ session, "MyClusterType" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).results.size(), 0);
            auto clusterTypes{ ClusterType::findOrphanIds(session) };
            ASSERT_EQ(clusterTypes.results.size(), 1);
            EXPECT_EQ(clusterTypes.results.front(), clusterType.getId());
        }

        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

        {
            auto transaction{ session.createReadTransaction() };
            auto clusters{ Cluster::findOrphanIds(session) };
            EXPECT_EQ(clusters.results.size(), 2);
            EXPECT_EQ(track->getClusters().size(), 0);
            EXPECT_EQ(track->getClusterIds().size(), 0);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster1.getId()), 0);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster2.getId()), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            cluster1.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto clusters{ Cluster::findIds(session, Cluster::FindParameters{}.setTrack(track.getId())) };
            ASSERT_EQ(clusters.results.size(), 1);
            EXPECT_EQ(clusters.results.front(), cluster1.getId());
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster1.getId()), 1);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster2.getId()), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            auto clusters{ Cluster::findOrphanIds(session) };
            ASSERT_EQ(clusters.results.size(), 1);
            EXPECT_EQ(clusters.results.front(), cluster2.getId());

            EXPECT_EQ(ClusterType::findOrphanIds(session).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster1.getId() }))) };
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results.front(), track.getId());

            tracks = Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster2.getId() })));
            EXPECT_EQ(tracks.results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto clusters{ track->getClusters() };
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front()->getId(), cluster1.getId());

            auto clusterIds{ track->getClusterIds() };
            ASSERT_EQ(clusterIds.size(), 1);
            EXPECT_EQ(clusterIds.front(), cluster1.getId());
        }
    }

    TEST_F(DatabaseFixture, Cluster_singleTrackWithSeveralClusters)
    {
        ScopedTrack track{ session };
        ScopedClusterType clusterType{ session, "MyClusterType" };

        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

        const std::vector<ClusterId> clusterIds{ cluster1.getId(), cluster2.getId() };

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setClusters(clusterIds))) };
            EXPECT_EQ(tracks.results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            cluster1.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setClusters(clusterIds))) };
            EXPECT_EQ(tracks.results.size(), 0);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster1.getId()), 1);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster2.getId()), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            cluster2.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto tracks{ Track::findIds(session, Track::FindParameters{}.setFilters(Filters{}.setClusters(clusterIds))) };
            ASSERT_EQ(tracks.results.size(), 1);
            EXPECT_EQ(tracks.results.front(), track.getId());
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster1.getId()), 1);
            EXPECT_EQ(Cluster::computeTrackCount(session, cluster2.getId()), 1);
        }
    }

    TEST_F(DatabaseFixture, Cluster_multiTracks)
    {
        std::list<ScopedTrack> tracks;
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        for (std::size_t i{}; i < 10; ++i)
        {
            tracks.emplace_back(session);

            {
                auto transaction{ session.createWriteTransaction() };
                cluster.get().modify()->addTrack(tracks.back().get());
            }
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).results.size(), 0);

            EXPECT_EQ(Cluster::computeTrackCount(session, cluster.getId()), tracks.size());

            for (TrackId trackId : cluster->getTracks().results)
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
            ClusterType::find(session, [&](const ClusterType::pointer&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        ScopedClusterType clusterType1{ session, "MyClusterType1" };
        ScopedClusterType clusterType2{ session, "MyClusterType2" };

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ClusterTypeId> visitedClusterTypes;
            ClusterType::find(session, [&](const ClusterType::pointer& clusterType) {
                visitedClusterTypes.push_back(clusterType->getId());
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
            EXPECT_EQ(Cluster::find(session, Cluster::FindParameters{}).results.size(), 0);
            EXPECT_EQ(Cluster::find(session, Cluster::FindParameters{}.setClusterTypeName("Foo")).results.size(), 0);
        }

        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        {
            auto transaction{ session.createReadTransaction() };
            auto clusters{ Cluster::findIds(session, Cluster::FindParameters{}).results };
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front(), cluster.getId());

            clusters = Cluster::findIds(session, Cluster::FindParameters{}.setClusterType(clusterType.getId())).results;
            ASSERT_EQ(clusters.size(), 1);
            EXPECT_EQ(clusters.front(), cluster.getId());

            clusters = Cluster::findIds(session, Cluster::FindParameters{}.setClusterTypeName("Foo")).results;
            EXPECT_EQ(clusters.size(), 0);

            clusters = Cluster::findIds(session, Cluster::FindParameters{}.setClusterTypeName("MyClusterType")).results;
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
            EXPECT_EQ(Cluster::findOrphanIds(session).results.size(), 0);
        }

        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };
        ScopedCluster unusedCluster{ session, clusterType.lockAndGet(), "MyClusterUnused" };

        {
            auto transaction{ session.createReadTransaction() };
            ASSERT_EQ(Cluster::findOrphanIds(session).results.size(), 2);
            EXPECT_EQ(Release::find(session, Release::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ unusedCluster.getId() }))).results.size(), 0);
            EXPECT_EQ(Release::find(session, Release::FindParameters{}).results.size(), 1);
            EXPECT_EQ(Cluster::computeReleaseCount(session, cluster.getId()), 0);
            EXPECT_EQ(Cluster::computeReleaseCount(session, unusedCluster.getId()), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            track.get().modify()->setRelease(release.get());
            cluster.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                auto clusters{ Cluster::findOrphanIds(session) };
                ASSERT_EQ(clusters.results.size(), 1);
                EXPECT_EQ(clusters.results.front(), unusedCluster.getId());
            }
            EXPECT_EQ(Cluster::computeReleaseCount(session, cluster.getId()), 1);
            EXPECT_EQ(Cluster::computeReleaseCount(session, unusedCluster.getId()), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto clusters{ Cluster::findIds(session, Cluster::FindParameters{}.setRelease(release.getId())) };
            ASSERT_EQ(clusters.results.size(), 1);
            EXPECT_EQ(clusters.results.front(), cluster.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster.getId() }))) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto releases{ Release::findIds(session, Release::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ unusedCluster.getId() }))) };
            EXPECT_EQ(releases.results.size(), 0);
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
        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "Cluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "Cluster2" };
        ScopedCluster cluster3{ session, clusterType.lockAndGet(), "Cluster3" };
        {
            auto transaction{ session.createWriteTransaction() };

            auto trackArtistLink{ TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist) };
            cluster1.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(ClusterType::findOrphanIds(session).results.size(), 0);
            EXPECT_EQ(Cluster::findOrphanIds(session).results.size(), 2);
            EXPECT_EQ(Release::findOrphanIds(session).results.size(), 0);
            EXPECT_EQ(Artist::findOrphanIds(session).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getClusters().size(), 1);
            EXPECT_EQ(track->getClusterIds().size(), 1);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster1.getId() }))) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());

            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster2.getId() }))).results.size(), 0);
            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster3.getId() }))).results.size(), 0);

            cluster2.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster1.getId() }))) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());

            artists = Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster2.getId() })));
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());

            artists = Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster1.getId() })));
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());

            EXPECT_EQ(Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster3.getId() }))).results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, SingleTrackSingleArtistMultiRolesMultiClusters)
    {
        ScopedTrack track{ session };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::ReleaseArtist);
            cluster.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).results.size(), 0);
            EXPECT_EQ(Release::findOrphanIds(session).results.size(), 0);
            EXPECT_EQ(Artist::findOrphanIds(session).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster.getId() }))) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());
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
            clusters.emplace_back(session, clusterType.lockAndGet(), "MyCluster" + std::to_string(i));

        for (std::size_t i{}; i < nbTracks; ++i)
        {
            tracks.emplace_back(session);

            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLinkType::Artist);

            for (auto& cluster : clusters)
                cluster.get().modify()->addTrack(tracks.back().get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Cluster::findOrphanIds(session).results.size(), 0);
            EXPECT_EQ(Artist::findOrphanIds(session).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<ClusterId> clusterIds;
            std::transform(std::cbegin(clusters), std::cend(clusters), std::back_inserter(clusterIds), [](const ScopedCluster& cluster) { return cluster.getId(); });

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(clusterIds))) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());
        }
    }

    TEST_F(DatabaseFixture, MultipleTracksSingleClusterSimilarity)
    {
        std::list<ScopedTrack> tracks;
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyClusterType" };

        for (std::size_t i{}; i < 10; ++i)
        {
            tracks.emplace_back(session);

            {
                auto transaction{ session.createWriteTransaction() };
                cluster.get().modify()->addTrack(tracks.back().get());
            }
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto similarTracks{ Track::findSimilarTrackIds(session, { tracks.front().getId() }) };
            EXPECT_EQ(similarTracks.results.size(), tracks.size() - 1);
            for (const TrackId similarTrackId : similarTracks.results)
            {
                EXPECT_TRUE(std::find_if(std::next(std::cbegin(tracks), 1), std::cend(tracks), [&](const auto& track) { return similarTrackId == track.getId(); }) != std::cend(tracks));
            }
        }
    }

    TEST_F(DatabaseFixture, MultipleTracksMultipleClustersSimilarity)
    {
        std::list<ScopedTrack> tracks;
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

        for (std::size_t i{}; i < 5; ++i)
        {
            tracks.emplace_back(session);

            {
                auto transaction{ session.createWriteTransaction() };
                cluster1.get().modify()->addTrack(tracks.back().get());
            }
        }

        for (std::size_t i{ 5 }; i < 10; ++i)
        {
            tracks.emplace_back(session);

            {
                auto transaction{ session.createWriteTransaction() };
                cluster1.get().modify()->addTrack(tracks.back().get());
                cluster2.get().modify()->addTrack(tracks.back().get());
            }
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                auto similarTracks{ Track::findSimilarTrackIds(session, { tracks.back().getId() }, Range{ 0, 4 }) };
                EXPECT_EQ(similarTracks.results.size(), 4);
                for (const TrackId similarTrackId : similarTracks.results)
                    EXPECT_TRUE(std::find_if(std::next(std::cbegin(tracks), 5), std::next(std::cend(tracks), -1), [&](const auto& track) { return similarTrackId == track.getId(); }) != std::cend(tracks));
            }

            {
                auto similarTracks{ Track::findSimilarTrackIds(session, { tracks.front().getId() }) };
                EXPECT_EQ(similarTracks.results.size(), tracks.size() - 1);
                for (const TrackId similarTrackId : similarTracks.results)
                    EXPECT_TRUE(std::find_if(std::next(std::cbegin(tracks), 1), std::cend(tracks), [&](const auto& track) { return similarTrackId == track.getId(); }) != std::cend(tracks));
            }
        }
    }

    TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtistSingleCluster)
    {
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedClusterType clusterType{ session, "MyType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        {
            auto transaction{ session.createWriteTransaction() };

            TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
            track.get().modify()->setRelease(release.get());
            cluster.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Cluster::findOrphanIds(session).results.size(), 0);
            EXPECT_EQ(ClusterType::findOrphanIds(session).results.size(), 0);
            EXPECT_EQ(Artist::findOrphanIds(session).results.size(), 0);
            EXPECT_EQ(Release::findOrphanIds(session).results.size(), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto artists{ Artist::findIds(session, Artist::FindParameters{}.setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster.getId() }))) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist.getId());

            auto releases{ Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId())) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId()).setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster.getId() })));
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
        }
    }

    TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtistMultiClusters)
    {
        ScopedTrack track{ session };
        ScopedRelease release{ session, "MyRelease" };
        ScopedArtist artist{ session, "MyArtist" };
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

        {
            auto transaction{ session.createWriteTransaction() };

            auto trackArtistLink{ TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist) };
            track.get().modify()->setRelease(release.get());
            cluster1.get().modify()->addTrack(track.get());
            cluster2.get().modify()->addTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto releases{ Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId())) };
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());

            releases = Release::findIds(session, Release::FindParameters{}.setArtist(artist.getId()).setFilters(Filters{}.setClusters(std::initializer_list<ClusterId>{ cluster1.getId(), cluster2.getId() })));
            ASSERT_EQ(releases.results.size(), 1);
            EXPECT_EQ(releases.results.front(), release.getId());
        }
    }

    TEST_F(DatabaseFixture, SingleTrackListMultipleTrackSingleCluster)
    {
        ScopedTrackList trackList{ session, "MyTrackList", TrackListType::PlayList };
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };
        std::list<ScopedTrack> tracks;

        for (std::size_t i{}; i < 20; ++i)
        {
            tracks.emplace_back(session);

            auto transaction{ session.createWriteTransaction() };

            if (i < 5)
                session.create<TrackListEntry>(tracks.back().get(), trackList.get());

            if (i < 10)
                cluster.get().modify()->addTrack(tracks.back().get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto similarTracks{ trackList->getSimilarTracks() };
            EXPECT_EQ(similarTracks.size(), 5);

            for (const auto& similarTrack : similarTracks)
                EXPECT_TRUE(std::any_of(std::next(std::cbegin(tracks), 5), std::cend(tracks), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack->getId(); }));
        }
    }

    TEST_F(DatabaseFixture, SingleTrackListMultipleTrackMultiClusters)
    {
        ScopedTrackList trackList{ session, "MyTrackList", TrackListType::PlayList };
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };
        std::list<ScopedTrack> tracks;

        for (std::size_t i{}; i < 20; ++i)
        {
            tracks.emplace_back(session);

            auto transaction{ session.createWriteTransaction() };

            if (i < 5)
                session.create<TrackListEntry>(tracks.back().get(), trackList.get());

            if (i < 10)
            {
                cluster1.get().modify()->addTrack(tracks.back().get());
                cluster2.get().modify()->addTrack(tracks.back().get());
            }
            else if (i < 15)
            {
                cluster1.get().modify()->addTrack(tracks.back().get());
            }
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                const auto similarTracks{ trackList->getSimilarTracks(0, 5) };
                ASSERT_EQ(similarTracks.size(), 5);

                for (auto similarTrack : similarTracks)
                    EXPECT_TRUE(std::any_of(std::next(std::cbegin(tracks), 5), std::next(std::cbegin(tracks), 10), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack->getId(); }));
            }

            {
                const auto similarTracks{ trackList->getSimilarTracks(5, 10) };
                ASSERT_EQ(similarTracks.size(), 5);

                for (auto similarTrack : similarTracks)
                    EXPECT_TRUE(std::any_of(std::next(std::cbegin(tracks), 10), std::next(std::cbegin(tracks), 15), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack->getId(); }));
            }

            EXPECT_EQ(trackList->getSimilarTracks(10, 10).size(), 0);
        }
    }

    TEST_F(DatabaseFixture, MultipleTracksMultipleArtistsMultiClusters)
    {
        ScopedArtist artist1{ session, "MyArtist1" };
        ScopedArtist artist2{ session, "MyArtist2" };
        ScopedArtist artist3{ session, "MyArtist3" };
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(artist1->findSimilarArtistIds().results.size(), 0);
            EXPECT_EQ(artist2->findSimilarArtistIds().results.size(), 0);
            EXPECT_EQ(artist3->findSimilarArtistIds().results.size(), 0);
        }

        std::list<ScopedTrack> tracks;
        for (std::size_t i{}; i < 10; ++i)
        {
            tracks.emplace_back(session);

            auto transaction{ session.createWriteTransaction() };

            if (i < 5)
                TrackArtistLink::create(session, tracks.back().get(), artist1.get(), TrackArtistLinkType::Artist);
            else
            {
                TrackArtistLink::create(session, tracks.back().get(), artist2.get(), TrackArtistLinkType::Artist);
                cluster2.get().modify()->addTrack(tracks.back().get());
            }

            cluster1.get().modify()->addTrack(tracks.back().get());
        }

        tracks.emplace_back(session);
        {
            auto transaction{ session.createWriteTransaction() };
            TrackArtistLink::create(session, tracks.back().get(), artist3.get(), TrackArtistLinkType::Artist);
            cluster2.get().modify()->addTrack(tracks.back().get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                auto artists{ artist1->findSimilarArtistIds() };
                ASSERT_EQ(artists.results.size(), 1);
                EXPECT_EQ(artists.results.front(), artist2.getId());
            }

            {
                auto artists{ artist1->findSimilarArtistIds({ TrackArtistLinkType::Artist }) };
                ASSERT_EQ(artists.results.size(), 1);
                EXPECT_EQ(artists.results.front(), artist2.getId());
            }

            {
                auto artists{ artist1->findSimilarArtistIds({ TrackArtistLinkType::ReleaseArtist }) };
                EXPECT_EQ(artists.results.size(), 0);
            }

            {
                auto artists{ artist1->findSimilarArtistIds({ TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist }) };
                ASSERT_EQ(artists.results.size(), 1);
                EXPECT_EQ(artists.results.front(), artist2.getId());
            }

            {
                auto artists{ artist1->findSimilarArtistIds({ TrackArtistLinkType::Composer }) };
                EXPECT_EQ(artists.results.size(), 0);
            }

            {
                auto artists{ artist2->findSimilarArtistIds() };
                ASSERT_EQ(artists.results.size(), 2);
                EXPECT_EQ(artists.results[0], artist1.getId());
                EXPECT_EQ(artists.results[1], artist3.getId());
            }
        }
    }

    TEST_F(DatabaseFixture, MultipleTracksMultipleReleasesMultiClusters)
    {
        ScopedRelease release1{ session, "MyRelease1" };
        ScopedRelease release2{ session, "MyRelease2" };
        ScopedRelease release3{ session, "MyRelease3" };
        ScopedClusterType clusterType{ session, "MyClusterType" };
        ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
        ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(release1->getSimilarReleases().size(), 0);
            EXPECT_EQ(release2->getSimilarReleases().size(), 0);
            EXPECT_EQ(release3->getSimilarReleases().size(), 0);
        }

        std::list<ScopedTrack> tracks;
        for (std::size_t i{}; i < 10; ++i)
        {
            tracks.emplace_back(session);

            auto transaction{ session.createWriteTransaction() };

            if (i < 5)
                tracks.back().get().modify()->setRelease(release1.get());
            else
            {
                tracks.back().get().modify()->setRelease(release2.get());
                cluster2.get().modify()->addTrack(tracks.back().get());
            }

            cluster1.get().modify()->addTrack(tracks.back().get());
        }

        tracks.emplace_back(session);
        {
            auto transaction{ session.createWriteTransaction() };
            tracks.back().get().modify()->setRelease(release3.get());
            cluster2.get().modify()->addTrack(tracks.back().get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            {
                auto releases{ release1->getSimilarReleases() };
                ASSERT_EQ(releases.size(), 1);
                EXPECT_EQ(releases.front()->getId(), release2.getId());
            }

            {
                auto releases{ release2->getSimilarReleases() };
                ASSERT_EQ(releases.size(), 2);
                EXPECT_EQ(releases[0]->getId(), release1.getId());
                EXPECT_EQ(releases[1]->getId(), release3.getId());
            }
        }
    }
} // namespace lms::db::tests