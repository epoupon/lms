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

using namespace Database;

TEST_F(DatabaseFixture, Cluster)
{
    {
        auto transaction{ session.createUniqueTransaction() };
        EXPECT_EQ(Cluster::getCount(session), 0);
        EXPECT_EQ(ClusterType::getCount(session), 0);
    }

    ScopedClusterType clusterType{ session, "MyType" };

    {
        auto transaction{ session.createUniqueTransaction() };
        EXPECT_EQ(ClusterType::getCount(session), 1);
    }

    {
        ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

        {
            auto transaction{ session.createUniqueTransaction() };

            EXPECT_EQ(Cluster::getCount(session), 1);
            EXPECT_EQ(cluster->getType()->getId(), clusterType.getId());

            {
                const auto clusters{ Cluster::find(session, Cluster::FindParameters {}) };
                ASSERT_EQ(clusters.results.size(), 1);
                EXPECT_EQ(std::get<ClusterId>(clusters.results.front()), cluster.getId());
            }

            {
                const auto clusters{ Cluster::findOrphans(session, Range{}) };
                ASSERT_EQ(clusters.results.size(), 1);
                EXPECT_EQ(clusters.results.front(), cluster.getId());
            }

            auto clusterTypes{ ClusterType::find(session, Range {}) };
            ASSERT_EQ(clusterTypes.results.size(), 1);
            EXPECT_EQ(clusterTypes.results.front(), clusterType.getId());

            clusterTypes = ClusterType::findUsed(session, Range{});
            ASSERT_EQ(clusterTypes.results.size(), 1);
            EXPECT_EQ(clusterTypes.results.front(), clusterType.getId());

            clusterTypes = ClusterType::findOrphans(session, Range{});
            EXPECT_TRUE(clusterTypes.results.empty());
        }
    }

    {
        auto transaction{ session.createUniqueTransaction() };

        auto clusterTypes{ ClusterType::findOrphans(session, Range {}) };
        ASSERT_EQ(clusterTypes.results.size(), 1);
        EXPECT_EQ(clusterTypes.results.front(), clusterType.getId());

        ASSERT_TRUE(ClusterType::findUsed(session, Range{}).results.empty());
    }
}

TEST_F(DatabaseFixture, Cluster_singleTrack)
{
    ScopedTrack track{ session, "MyTrack" };
    ScopedClusterType clusterType{ session, "MyClusterType" };

    {
        auto transaction{ session.createSharedTransaction() };
        EXPECT_TRUE(Cluster::findOrphans(session, Range{}).results.empty());
        auto clusterTypes{ ClusterType::findOrphans(session, Range {}) };
        ASSERT_EQ(clusterTypes.results.size(), 1);
        EXPECT_EQ(clusterTypes.results.front(), clusterType.getId());
    }

    ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
    ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

    {
        auto transaction{ session.createSharedTransaction() };
        auto clusters{ Cluster::findOrphans(session, Range {}) };
        EXPECT_EQ(clusters.results.size(), 2);
        EXPECT_TRUE(track->getClusters().empty());
        EXPECT_TRUE(track->getClusterIds().empty());
    }

    {
        auto transaction{ session.createUniqueTransaction() };

        cluster1.get().modify()->addTrack(track.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };
        auto clusters{ Cluster::find(session, Cluster::FindParameters {}.setTrack(track.getId())) };
        ASSERT_EQ(clusters.results.size(), 1);
        EXPECT_EQ(std::get<ClusterId>(clusters.results.front()), cluster1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };
        auto clusters{ Cluster::findOrphans(session, Range {}) };
        ASSERT_EQ(clusters.results.size(), 1);
        EXPECT_EQ(clusters.results.front(), cluster2.getId());

        EXPECT_TRUE(ClusterType::findOrphans(session, Range{}).results.empty());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto tracks{ Track::find(session, Track::FindParameters {}.setClusters({cluster1.getId()})) };
        ASSERT_EQ(tracks.results.size(), 1);
        EXPECT_EQ(tracks.results.front(), track.getId());

        tracks = Track::find(session, Track::FindParameters{}.setClusters({ cluster2.getId() }));
        EXPECT_TRUE(tracks.results.empty());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto clusters{ track->getClusters() };
        ASSERT_EQ(clusters.size(), 1);
        EXPECT_EQ(clusters.front()->getId(), cluster1.getId());

        auto clusterIds{ track->getClusterIds() };
        ASSERT_EQ(clusterIds.size(), 1);
        EXPECT_EQ(clusterIds.front(), cluster1.getId());
    }
}

TEST_F(DatabaseFixture, Cluster_multiTracks)
{
    std::list<ScopedTrack> tracks;
    ScopedClusterType clusterType{ session, "MyClusterType" };
    ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

    for (std::size_t i{}; i < 10; ++i)
    {
        tracks.emplace_back(session, "MyTrack" + std::to_string(i));

        {
            auto transaction{ session.createUniqueTransaction() };
            cluster.get().modify()->addTrack(tracks.back().get());
        }
    }

    {
        auto transaction{ session.createSharedTransaction() };
        EXPECT_TRUE(Cluster::findOrphans(session, Range{}).results.empty());

        EXPECT_EQ(cluster->getTracksCount(), tracks.size());

        for (TrackId trackId : cluster->getTracks(Range{}).results)
        {
            auto it{ std::find_if(std::cbegin(tracks), std::cend(tracks), [&](const ScopedTrack& track) { return trackId == track.getId(); }) };
            EXPECT_TRUE(it != std::cend(tracks));
        }
    }
}

TEST_F(DatabaseFixture, Cluster_multiTracksMultipleClustersTopRelease)
{
    ScopedClusterType clusterType{ session, "ClusterType" };
    ScopedCluster cluster1{ session, clusterType.lockAndGet(), "Cluster1" };
    ScopedCluster cluster2{ session, clusterType.lockAndGet(), "Cluster2" };
    ScopedCluster cluster3{ session, clusterType.lockAndGet(), "Cluster3" };
    ScopedTrack trackA{ session, "TrackA" };
    ScopedTrack trackB{ session, "TrackB" };
    ScopedTrack trackC{ session, "TrackC" };
    ScopedRelease releaseA{ session, "ReleaseA" };
    ScopedRelease releaseB{ session, "ReleaseB" };
    ScopedRelease releaseC{ session, "ReleaseC" };

    ScopedUser user{ session, "MyUser" };
    ScopedTrackList trackList{ session, "TrackList", TrackListType::Playlist, false, user.lockAndGet() };

    {
        auto transaction{ session.createSharedTransaction() };

        EXPECT_EQ(trackList->getDuration(), std::chrono::seconds{ 0 });
    }

    {
        auto transaction{ session.createUniqueTransaction() };

        cluster1.get().modify()->addTrack(trackA.get());
        cluster2.get().modify()->addTrack(trackB.get());
        cluster2.get().modify()->addTrack(trackC.get());
        cluster3.get().modify()->addTrack(trackC.get());

        trackA.get().modify()->setRelease(releaseA.get());
        trackB.get().modify()->setRelease(releaseB.get());
        trackC.get().modify()->setRelease(releaseC.get());
    }

    {
        auto transaction{ session.createUniqueTransaction() };

        session.create<TrackListEntry>(trackA.get(), trackList.get());
        session.create<TrackListEntry>(trackB.get(), trackList.get());
        session.create<TrackListEntry>(trackB.get(), trackList.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool hasMore;
        const auto releases{ trackList->getTopReleases({}, std::nullopt, hasMore) };
        ASSERT_EQ(releases.size(), 2);
        EXPECT_EQ(releases[0]->getId(), releaseB.getId());
        EXPECT_EQ(releases[1]->getId(), releaseA.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool hasMore;
        auto releases{ trackList->getTopReleases({cluster1.getId()}, std::nullopt, hasMore) };
        ASSERT_EQ(releases.size(), 1);
        EXPECT_EQ(releases[0]->getId(), releaseA.getId());

        releases = trackList->getTopReleases({ cluster2.getId() }, std::nullopt, hasMore);
        ASSERT_EQ(releases.size(), 1);
        EXPECT_EQ(releases[0]->getId(), releaseB.getId());

        releases = trackList->getTopReleases({ cluster2.getId(), cluster1.getId() }, std::nullopt, hasMore);
        EXPECT_TRUE(releases.empty());

        releases = trackList->getTopReleases({ cluster2.getId(), cluster3.getId() }, std::nullopt, hasMore);
        EXPECT_TRUE(releases.empty());
    }


    {
        auto transaction{ session.createUniqueTransaction() };

        session.create<TrackListEntry>(trackC.get(), trackList.get());
        session.create<TrackListEntry>(trackC.get(), trackList.get());
        session.create<TrackListEntry>(trackC.get(), trackList.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool hasMore;
        auto releases{ trackList->getTopReleases({cluster2.getId(), cluster3.getId()}, std::nullopt, hasMore) };
        ASSERT_EQ(releases.size(), 1);
        EXPECT_EQ(releases[0]->getId(), releaseC.getId());

        releases = trackList->getTopReleases({ cluster2.getId() }, std::nullopt, hasMore);
        ASSERT_EQ(releases.size(), 2);
        EXPECT_EQ(releases[0]->getId(), releaseC.getId());
        EXPECT_EQ(releases[1]->getId(), releaseB.getId());
    }
}

TEST_F(DatabaseFixture, Cluster_singleTrackSingleReleaseSingleCluster)
{
    ScopedTrack track{ session, "MyTrackFile" };
    ScopedRelease release{ session, "MyRelease" };

    {
        auto transaction{ session.createSharedTransaction() };
        EXPECT_TRUE(Cluster::findOrphans(session, Range{}).results.empty());
    }

    ScopedClusterType clusterType{ session, "MyClusterType" };
    ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };
    ScopedCluster unusedCluster{ session, clusterType.lockAndGet(), "MyClusterUnused" };

    {
        auto transaction{ session.createSharedTransaction() };
        ASSERT_EQ(Cluster::findOrphans(session, Range{}).results.size(), 2);
        EXPECT_TRUE(Release::find(session, Release::FindParameters{}.setClusters({ unusedCluster.getId() })).results.empty());
        EXPECT_EQ(Release::find(session, Release::FindParameters{}).results.size(), 1);
    }

    {
        auto transaction{ session.createUniqueTransaction() };

        track.get().modify()->setRelease(release.get());
        cluster.get().modify()->addTrack(track.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        {
            auto clusters{ Cluster::findOrphans(session, Range {}) };
            ASSERT_EQ(clusters.results.size(), 1);
            EXPECT_EQ(clusters.results.front(), unusedCluster.getId());
        }
    }

    {
        auto transaction{ session.createSharedTransaction() };

        const auto clusters{ Cluster::find(session, Cluster::FindParameters{}.setRelease(release.getId())) };
        ASSERT_EQ(clusters.results.size(), 1);
        EXPECT_EQ(std::get<ClusterId>(clusters.results.front()), cluster.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto releases{ Release::find(session, Release::FindParameters {}.setClusters({cluster.getId()})) };
        ASSERT_EQ(releases.results.size(), 1);
        EXPECT_EQ(releases.results.front(), release.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto releases{ Release::find(session, Release::FindParameters {}.setClusters({unusedCluster.getId()})) };
        EXPECT_EQ(releases.results.size(), 0);
    }

    {
        auto transaction{ session.createSharedTransaction() };

        EXPECT_EQ(cluster->getReleasesCount(), 1);
        EXPECT_EQ(cluster->getTracksCount(), 1);
        EXPECT_EQ(unusedCluster->getReleasesCount(), 0);
        EXPECT_EQ(unusedCluster->getTracksCount(), 0);
    }
}

TEST_F(DatabaseFixture, SingleTrackSingleArtistMultiClusters)
{
    ScopedTrack track{ session, "MyTrackFile" };
    ScopedArtist artist{ session, "MyArtist" };
    ScopedClusterType clusterType{ session, "MyType" };
    ScopedCluster cluster1{ session, clusterType.lockAndGet(), "Cluster1" };
    ScopedCluster cluster2{ session, clusterType.lockAndGet(), "Cluster2" };
    ScopedCluster cluster3{ session, clusterType.lockAndGet(), "Cluster3" };
    {
        auto transaction{ session.createUniqueTransaction() };

        auto trackArtistLink{ TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist) };
        cluster1.get().modify()->addTrack(track.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };
        EXPECT_TRUE(ClusterType::findOrphans(session, Range{}).results.empty());
        EXPECT_EQ(Cluster::findOrphans(session, Range{}).results.size(), 2);
        EXPECT_TRUE(Release::findOrphans(session, Range{}).results.empty());
        EXPECT_TRUE(Artist::findAllOrphans(session, Range{}).results.empty());
    }

    {
        auto transaction{ session.createSharedTransaction() };
        EXPECT_EQ(track->getClusters().size(), 1);
        EXPECT_EQ(track->getClusterIds().size(), 1);
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto artists{ Artist::find(session, Artist::FindParameters {}.setClusters({cluster1.getId()})) };
        ASSERT_EQ(artists.results.size(), 1);
        EXPECT_EQ(artists.results.front(), artist.getId());

        EXPECT_TRUE(Artist::find(session, Artist::FindParameters{}.setClusters({ cluster2.getId() })).results.empty());
        EXPECT_TRUE(Artist::find(session, Artist::FindParameters{}.setClusters({ cluster3.getId() })).results.empty());

        cluster2.get().modify()->addTrack(track.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto artists{ Artist::find(session, Artist::FindParameters {}.setClusters({cluster1.getId()})) };
        ASSERT_EQ(artists.results.size(), 1);
        EXPECT_EQ(artists.results.front(), artist.getId());

        artists = Artist::find(session, Artist::FindParameters{}.setClusters({ cluster2.getId() }));
        ASSERT_EQ(artists.results.size(), 1);
        EXPECT_EQ(artists.results.front(), artist.getId());

        artists = Artist::find(session, Artist::FindParameters{}.setClusters({ cluster1.getId() }));
        ASSERT_EQ(artists.results.size(), 1);
        EXPECT_EQ(artists.results.front(), artist.getId());

        EXPECT_TRUE(Artist::find(session, Artist::FindParameters{}.setClusters({ cluster3.getId() })).results.empty());
    }
}

TEST_F(DatabaseFixture, SingleTrackSingleArtistMultiRolesMultiClusters)
{
    ScopedTrack track{ session, "MyTrackFile" };
    ScopedArtist artist{ session, "MyArtist" };
    ScopedClusterType clusterType{ session, "MyType" };
    ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

    {
        auto transaction{ session.createUniqueTransaction() };

        TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::ReleaseArtist);
        cluster.get().modify()->addTrack(track.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };
        EXPECT_TRUE(Cluster::findOrphans(session, Range{}).results.empty());
        EXPECT_TRUE(Release::findOrphans(session, Range{}).results.empty());
        EXPECT_TRUE(Artist::findAllOrphans(session, Range{}).results.empty());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto artists{ Artist::find(session, Artist::FindParameters {}.setClusters({cluster.getId()})) };
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
        tracks.emplace_back(session, "MyTrackFile" + std::to_string(i));

        auto transaction{ session.createUniqueTransaction() };
        TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLinkType::Artist);

        for (auto& cluster : clusters)
            cluster.get().modify()->addTrack(tracks.back().get());
    }

    {
        auto transaction{ session.createSharedTransaction() };
        EXPECT_TRUE(Cluster::findOrphans(session, Range{}).results.empty());
        EXPECT_TRUE(Artist::findAllOrphans(session, Range{}).results.empty());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        std::vector<ClusterId> clusterIds;
        std::transform(std::cbegin(clusters), std::cend(clusters), std::back_inserter(clusterIds), [](const ScopedCluster& cluster) { return cluster.getId(); });

        auto artists{ Artist::find(session, Artist::FindParameters {}.setClusters(clusterIds)) };
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
        tracks.emplace_back(session, "MyTrack" + std::to_string(i));

        {
            auto transaction{ session.createUniqueTransaction() };
            cluster.get().modify()->addTrack(tracks.back().get());
        }
    }

    {
        auto transaction{ session.createSharedTransaction() };

        const auto similarTracks{ Track::findSimilarTracks(session, {tracks.front().getId()}, Range {}) };
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
        tracks.emplace_back(session, "MyTrack" + std::to_string(i));

        {
            auto transaction{ session.createUniqueTransaction() };
            cluster1.get().modify()->addTrack(tracks.back().get());
        }
    }

    for (std::size_t i{ 5 }; i < 10; ++i)
    {
        tracks.emplace_back(session, "MyTrack" + std::to_string(i));

        {
            auto transaction{ session.createUniqueTransaction() };
            cluster1.get().modify()->addTrack(tracks.back().get());
            cluster2.get().modify()->addTrack(tracks.back().get());
        }
    }

    {
        auto transaction{ session.createSharedTransaction() };

        {
            auto similarTracks{ Track::findSimilarTracks(session, {tracks.back().getId()}, Range {0, 4}) };
            EXPECT_EQ(similarTracks.results.size(), 4);
            for (const TrackId similarTrackId : similarTracks.results)
                EXPECT_TRUE(std::find_if(std::next(std::cbegin(tracks), 5), std::next(std::cend(tracks), -1), [&](const auto& track) { return similarTrackId == track.getId(); }) != std::cend(tracks));
        }

        {
            auto similarTracks{ Track::findSimilarTracks(session, {tracks.front().getId()}, Range {}) };
            EXPECT_EQ(similarTracks.results.size(), tracks.size() - 1);
            for (const TrackId similarTrackId : similarTracks.results)
                EXPECT_TRUE(std::find_if(std::next(std::cbegin(tracks), 1), std::cend(tracks), [&](const auto& track) { return similarTrackId == track.getId(); }) != std::cend(tracks));
        }
    }
}

TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtistSingleCluster)
{
    ScopedTrack track{ session, "MyTrack" };
    ScopedRelease release{ session, "MyRelease" };
    ScopedArtist artist{ session, "MyArtist" };
    ScopedClusterType clusterType{ session, "MyType" };
    ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };

    {
        auto transaction{ session.createUniqueTransaction() };

        TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
        track.get().modify()->setRelease(release.get());
        cluster.get().modify()->addTrack(track.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        EXPECT_TRUE(Cluster::findOrphans(session, Range{}).results.empty());
        EXPECT_TRUE(ClusterType::findOrphans(session, Range{}).results.empty());
        EXPECT_TRUE(Artist::findAllOrphans(session, Range{}).results.empty());
        EXPECT_TRUE(Release::findOrphans(session, Range{}).results.empty());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto artists{ Artist::find(session, Artist::FindParameters {}.setClusters({cluster.getId()})) };
        ASSERT_EQ(artists.results.size(), 1);
        EXPECT_EQ(artists.results.front(), artist.getId());

        auto releases{ Release::find(session, Release::FindParameters {}.setArtist(artist.getId())) };
        ASSERT_EQ(releases.results.size(), 1);
        EXPECT_EQ(releases.results.front(), release.getId());

        releases = Release::find(session, Release::FindParameters{}.setArtist(artist.getId()).setClusters({ cluster.getId() }));
        ASSERT_EQ(releases.results.size(), 1);
        EXPECT_EQ(releases.results.front(), release.getId());
    }
}

TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtistMultiClusters)
{
    ScopedTrack track{ session, "MyTrack" };
    ScopedRelease release{ session, "MyRelease" };
    ScopedArtist artist{ session, "MyArtist" };
    ScopedClusterType clusterType{ session, "MyClusterType" };
    ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
    ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };

    {
        auto transaction{ session.createUniqueTransaction() };

        auto trackArtistLink{ TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist) };
        track.get().modify()->setRelease(release.get());
        cluster1.get().modify()->addTrack(track.get());
        cluster2.get().modify()->addTrack(track.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        auto releases{ Release::find(session, Release::FindParameters {}.setArtist(artist.getId())) };
        ASSERT_EQ(releases.results.size(), 1);
        EXPECT_EQ(releases.results.front(), release.getId());

        releases = Release::find(session, Release::FindParameters{}.setArtist(artist.getId()).setClusters({ cluster1.getId(), cluster2.getId() }));
        ASSERT_EQ(releases.results.size(), 1);
        EXPECT_EQ(releases.results.front(), release.getId());
    }
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackSingleCluster)
{
    ScopedUser user{ session, "MyUser" };
    ScopedTrackList trackList{ session, "MyTrackList", TrackListType::Playlist, false, user.lockAndGet() };
    ScopedClusterType clusterType{ session, "MyClusterType" };
    ScopedCluster cluster{ session, clusterType.lockAndGet(), "MyCluster" };
    std::list<ScopedTrack> tracks;

    for (std::size_t i{}; i < 20; ++i)
    {
        tracks.emplace_back(session, "MyTrack" + std::to_string(i));

        auto transaction{ session.createUniqueTransaction() };

        if (i < 5)
            session.create<TrackListEntry>(tracks.back().get(), trackList.get());

        if (i < 10)
            cluster.get().modify()->addTrack(tracks.back().get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        const auto similarTracks{ trackList->getSimilarTracks() };
        EXPECT_EQ(similarTracks.size(), 5);

        for (auto similarTrack : similarTracks)
            EXPECT_TRUE(std::any_of(std::next(std::cbegin(tracks), 5), std::cend(tracks), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack->getId(); }));
    }
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackMultiClusters)
{
    ScopedUser user{ session, "MyUser" };
    ScopedTrackList trackList{ session, "MyTrackList", TrackListType::Playlist, false, user.lockAndGet() };
    ScopedClusterType clusterType{ session, "MyClusterType" };
    ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
    ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };
    std::list<ScopedTrack> tracks;

    for (std::size_t i{}; i < 20; ++i)
    {
        tracks.emplace_back(session, "MyTrack" + std::to_string(i));

        auto transaction{ session.createUniqueTransaction() };

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
        auto transaction{ session.createSharedTransaction() };

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

        EXPECT_TRUE(trackList->getSimilarTracks(10, 10).empty());
    }
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackMultiClustersRecentlyPlayed)
{
    ScopedUser user{ session, "MyUser" };
    ScopedTrackList trackList{ session, "MyTrackList", TrackListType::Playlist, false, user.lockAndGet() };
    ScopedClusterType clusterType{ session, "MyClusterType" };
    ScopedCluster cluster1{ session, clusterType.lockAndGet(), "MyCluster1" };
    ScopedCluster cluster2{ session, clusterType.lockAndGet(), "MyCluster2" };
    ScopedCluster cluster3{ session, clusterType.lockAndGet(), "MyCluster3" };
    ScopedTrack track1{ session, "MyTrack1" };
    ScopedTrack track2{ session, "MyTrack1" };
    ScopedArtist artist1{ session, "MyArtist1" };
    ScopedArtist artist2{ session, "MyArtist2" };
    ScopedRelease release1{ session, "MyRelease1" };
    ScopedRelease release2{ session, "MyRelease2" };

    const Wt::WDateTime now{ Wt::WDateTime::currentDateTime() };

    {
        auto transaction{ session.createUniqueTransaction() };

        track1.get().modify()->setRelease(release1.get());
        track2.get().modify()->setRelease(release2.get());
        TrackArtistLink::create(session, track1.get(), artist1.get(), TrackArtistLinkType::Artist);
        TrackArtistLink::create(session, track2.get(), artist2.get(), TrackArtistLinkType::Artist);

        cluster1.get().modify()->addTrack(track1.get());
        cluster2.get().modify()->addTrack(track2.get());
        cluster3.get().modify()->addTrack(track1.get());
        cluster3.get().modify()->addTrack(track2.get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        EXPECT_TRUE(trackList->getArtistsOrderedByRecentFirst({}, std::nullopt, std::nullopt, moreResults).empty());
        EXPECT_TRUE(trackList->getReleasesOrderedByRecentFirst({}, std::nullopt, moreResults).empty());
        EXPECT_TRUE(trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults).empty());
    }

    {
        auto transaction{ session.createUniqueTransaction() };

        session.create<TrackListEntry>(track1.get(), trackList.get(), now);
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 1);
        EXPECT_EQ(artists.front()->getId(), artist1.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 1);
        EXPECT_EQ(releases.front()->getId(), release1.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 1);
        EXPECT_EQ(tracks.front()->getId(), track1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({cluster1.getId()}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 1);
        EXPECT_EQ(artists.front()->getId(), artist1.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({cluster1.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 1);
        EXPECT_EQ(releases.front()->getId(), release1.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({cluster1.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 1);
        EXPECT_EQ(tracks.front()->getId(), track1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({cluster3.getId()}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 1);
        EXPECT_EQ(artists.front()->getId(), artist1.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({cluster3.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 1);
        EXPECT_EQ(releases.front()->getId(), release1.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({cluster3.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 1);
        EXPECT_EQ(tracks.front()->getId(), track1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({cluster1.getId()}, TrackArtistLinkType::Artist, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 1);
        EXPECT_EQ(artists.front()->getId(), artist1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({}, TrackArtistLinkType::Artist, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 1);
        EXPECT_EQ(artists.front()->getId(), artist1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        EXPECT_TRUE(trackList->getArtistsOrderedByRecentFirst({ cluster2.getId() }, std::nullopt, std::nullopt, moreResults).empty());
        EXPECT_TRUE(trackList->getReleasesOrderedByRecentFirst({ cluster2.getId() }, std::nullopt, moreResults).empty());
        EXPECT_TRUE(trackList->getTracksOrderedByRecentFirst({ cluster2.getId() }, std::nullopt, moreResults).empty());

        EXPECT_TRUE(trackList->getArtistsOrderedByRecentFirst({}, TrackArtistLinkType::ReleaseArtist, std::nullopt, moreResults).empty());
    }

    {
        auto transaction{ session.createUniqueTransaction() };

        session.create<TrackListEntry>(track2.get(), trackList.get(), now.addSecs(1));
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 2);
        EXPECT_EQ(artists[0]->getId(), artist2.getId());
        EXPECT_EQ(artists[1]->getId(), artist1.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 2);
        EXPECT_EQ(releases[0]->getId(), release2.getId());
        EXPECT_EQ(releases[1]->getId(), release1.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 2);
        EXPECT_EQ(tracks[0]->getId(), track2.getId());
        EXPECT_EQ(tracks[1]->getId(), track1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({cluster3.getId()}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 2);
        EXPECT_EQ(artists[0]->getId(), artist2.getId());
        EXPECT_EQ(artists[1]->getId(), artist1.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({cluster3.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 2);
        EXPECT_EQ(releases[0]->getId(), release2.getId());
        EXPECT_EQ(releases[1]->getId(), release1.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({cluster3.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 2);
        EXPECT_EQ(tracks[0]->getId(), track2.getId());
        EXPECT_EQ(tracks[1]->getId(), track1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({cluster1.getId()}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 1);
        EXPECT_EQ(artists.front()->getId(), artist1.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({cluster1.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 1);
        EXPECT_EQ(releases.front()->getId(), release1.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({cluster1.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 1);
        EXPECT_EQ(tracks.front()->getId(), track1.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({cluster2.getId()}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 1);
        EXPECT_EQ(artists.front()->getId(), artist2.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({cluster2.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 1);
        EXPECT_EQ(releases.front()->getId(), release2.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({cluster2.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 1);
        EXPECT_EQ(tracks.front()->getId(), track2.getId());
    }

    {
        auto transaction{ session.createUniqueTransaction() };

        session.create<TrackListEntry>(track1.get(), trackList.get(), now.addSecs(2));
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 2);
        EXPECT_EQ(artists[0]->getId(), artist1.getId());
        EXPECT_EQ(artists[1]->getId(), artist2.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 2);
        EXPECT_EQ(releases[0]->getId(), release1.getId());
        EXPECT_EQ(releases[1]->getId(), release2.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 2);
        EXPECT_EQ(tracks[0]->getId(), track1.getId());
        EXPECT_EQ(tracks[1]->getId(), track2.getId());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        bool moreResults{};
        const auto artists{ trackList->getArtistsOrderedByRecentFirst({cluster3.getId()}, std::nullopt, std::nullopt, moreResults) };
        ASSERT_EQ(artists.size(), 2);
        EXPECT_EQ(artists[0]->getId(), artist1.getId());
        EXPECT_EQ(artists[1]->getId(), artist2.getId());

        const auto releases{ trackList->getReleasesOrderedByRecentFirst({cluster3.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(releases.size(), 2);
        EXPECT_EQ(releases[0]->getId(), release1.getId());
        EXPECT_EQ(releases[1]->getId(), release2.getId());

        const auto tracks{ trackList->getTracksOrderedByRecentFirst({cluster3.getId()}, std::nullopt, moreResults) };
        ASSERT_EQ(tracks.size(), 2);
        EXPECT_EQ(tracks[0]->getId(), track1.getId());
        EXPECT_EQ(tracks[1]->getId(), track2.getId());
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
        auto transaction{ session.createSharedTransaction() };
        EXPECT_TRUE(artist1->findSimilarArtists().results.empty());
        EXPECT_TRUE(artist2->findSimilarArtists().results.empty());
        EXPECT_TRUE(artist3->findSimilarArtists().results.empty());
    }

    std::list<ScopedTrack> tracks;
    for (std::size_t i{}; i < 10; ++i)
    {
        tracks.emplace_back(session, "MyTrack" + std::to_string(i));

        auto transaction{ session.createUniqueTransaction() };

        if (i < 5)
            TrackArtistLink::create(session, tracks.back().get(), artist1.get(), TrackArtistLinkType::Artist);
        else
        {
            TrackArtistLink::create(session, tracks.back().get(), artist2.get(), TrackArtistLinkType::Artist);
            cluster2.get().modify()->addTrack(tracks.back().get());
        }

        cluster1.get().modify()->addTrack(tracks.back().get());
    }

    tracks.emplace_back(session, "MyTrack" + std::to_string(tracks.size()));
    {
        auto transaction{ session.createUniqueTransaction() };
        TrackArtistLink::create(session, tracks.back().get(), artist3.get(), TrackArtistLinkType::Artist);
        cluster2.get().modify()->addTrack(tracks.back().get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

        {
            auto artists{ artist1->findSimilarArtists() };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist2.getId());
        }

        {
            auto artists{ artist1->findSimilarArtists({TrackArtistLinkType::Artist}) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist2.getId());
        }

        {
            auto artists{ artist1->findSimilarArtists({TrackArtistLinkType::ReleaseArtist}) };
            EXPECT_EQ(artists.results.empty(), 1);
        }

        {
            auto artists{ artist1->findSimilarArtists({TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist}) };
            ASSERT_EQ(artists.results.size(), 1);
            EXPECT_EQ(artists.results.front(), artist2.getId());
        }

        {
            auto artists{ artist1->findSimilarArtists({TrackArtistLinkType::Composer}) };
            EXPECT_TRUE(artists.results.empty());
        }

        {
            auto artists{ artist2->findSimilarArtists() };
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
        auto transaction{ session.createSharedTransaction() };
        EXPECT_TRUE(release1->getSimilarReleases().empty());
        EXPECT_TRUE(release2->getSimilarReleases().empty());
        EXPECT_TRUE(release3->getSimilarReleases().empty());
    }

    std::list<ScopedTrack> tracks;
    for (std::size_t i{}; i < 10; ++i)
    {
        tracks.emplace_back(session, "MyTrack" + std::to_string(i));

        auto transaction{ session.createUniqueTransaction() };

        if (i < 5)
            tracks.back().get().modify()->setRelease(release1.get());
        else
        {
            tracks.back().get().modify()->setRelease(release2.get());
            cluster2.get().modify()->addTrack(tracks.back().get());
        }

        cluster1.get().modify()->addTrack(tracks.back().get());
    }

    tracks.emplace_back(session, "MyTrack" + std::to_string(tracks.size()));
    {
        auto transaction{ session.createUniqueTransaction() };
        tracks.back().get().modify()->setRelease(release3.get());
        cluster2.get().modify()->addTrack(tracks.back().get());
    }

    {
        auto transaction{ session.createSharedTransaction() };

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
