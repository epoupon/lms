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

TEST_F(DatabaseFixture, SingleCluster)
{
	ScopedClusterType clusterType {session, "MyType"};

	{
		ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};

		{
			auto transaction {session.createUniqueTransaction()};

			auto clusters {Cluster::getAll(session)};
			ASSERT_EQ(clusters.size(), 1);
			EXPECT_EQ(clusters.front().id(), cluster.getId());
			EXPECT_EQ(clusters.front()->getType().id(), clusterType.getId());

			clusters = Cluster::getAllOrphans(session);
			ASSERT_EQ(clusters.size(), 1);
			EXPECT_EQ(clusters.front().id(), cluster.getId());

			auto clusterTypes {ClusterType::getAll(session)};
			ASSERT_EQ(clusterTypes.size(), 1);
			EXPECT_EQ(clusterTypes.front().id(), clusterType.getId());

			clusterTypes = ClusterType::getAllUsed(session);
			ASSERT_EQ(clusterTypes.size(), 1);
			EXPECT_EQ(clusterTypes.front().id(), clusterType.getId());

			clusterTypes = ClusterType::getAllOrphans(session);
			EXPECT_TRUE(clusterTypes.empty());
		}
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto clusterTypes {ClusterType::getAllOrphans(session)};
		ASSERT_EQ(clusterTypes.size(), 1);
		EXPECT_EQ(clusterTypes.front().id(), clusterType.getId());

		ASSERT_TRUE(ClusterType::getAllUsed(session).empty());
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleCluster)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedClusterType clusterType {session, "MyClusterType"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Cluster::getAllOrphans(session).empty());
		auto clusterTypes {ClusterType::getAllOrphans(session)};
		ASSERT_EQ(clusterTypes.size(), 1);
		EXPECT_EQ(clusterTypes.front().id(), clusterType.getId());
	}

	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	{
		auto transaction {session.createSharedTransaction()};
		auto clusters {Cluster::getAllOrphans(session)};
		EXPECT_EQ(clusters.size(), 2);
		EXPECT_TRUE(track->getClusters().empty());
		EXPECT_TRUE(track->getClusterIds().empty());
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Track::getAllIdsWithClusters(session).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		cluster1.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto tracks {Track::getAllIdsWithClusters(session)};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front(), track.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto clusters {Cluster::getAllOrphans(session)};
		ASSERT_EQ(clusters.size(), 1);
		EXPECT_EQ(clusters.front().id(), cluster2.getId());

		EXPECT_TRUE(ClusterType::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto tracks {Track::getByClusters(session, {cluster1.getId()})};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front().id(), track.getId());

		tracks = Track::getByClusters(session, {cluster2.getId()});
		EXPECT_TRUE(tracks.empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto clusters {track->getClusters()};
		ASSERT_EQ(clusters.size(), 1);
		EXPECT_EQ(clusters.front().id(), cluster1.getId());

		auto clusterIds {track->getClusterIds()};
		ASSERT_EQ(clusterIds.size(), 1);
		EXPECT_EQ(clusterIds.front(), cluster1.getId());
	}
}

TEST_F(DatabaseFixture, MultipleTracksSingleCluster)
{
	std::list<ScopedTrack> tracks;
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};

	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		{
			auto transaction {session.createUniqueTransaction()};
			cluster.get().modify()->addTrack(tracks.back().get());
		}
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Cluster::getAllOrphans(session).empty());

		EXPECT_EQ(cluster->getTracksCount(), tracks.size());

		for (auto trackCluster : cluster->getTracks())
		{
			auto it {std::find_if(std::cbegin(tracks), std::cend(tracks), [&](const ScopedTrack& track) { return trackCluster.id() == track.getId(); })};
			EXPECT_TRUE(it != std::cend(tracks));
		}
	}
}

TEST_F(DatabaseFixture, MultipleTracksMultipleClustersTopRelease)
{
	ScopedClusterType clusterType {session, "ClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "Cluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "Cluster2"};
	ScopedCluster cluster3 {session, clusterType.lockAndGet(), "Cluster3"};
	ScopedTrack trackA {session, "TrackA"};
	ScopedTrack trackB {session, "TrackB"};
	ScopedTrack trackC {session, "TrackC"};
	ScopedRelease releaseA {session, "ReleaseA"};
	ScopedRelease releaseB {session, "ReleaseB"};
	ScopedRelease releaseC {session, "ReleaseC"};

	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "TrackList", TrackList::Type::Playlist, false, user.lockAndGet()};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(trackList->getDuration(), std::chrono::seconds {0});
	}

	{
		auto transaction {session.createUniqueTransaction()};

		cluster1.get().modify()->addTrack(trackA.get());
		cluster2.get().modify()->addTrack(trackB.get());
		cluster2.get().modify()->addTrack(trackC.get());
		cluster3.get().modify()->addTrack(trackC.get());

		trackA.get().modify()->setRelease(releaseA.get());
		trackB.get().modify()->setRelease(releaseB.get());
		trackC.get().modify()->setRelease(releaseC.get());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, trackA.get(), trackList.get());
		TrackListEntry::create(session, trackB.get(), trackList.get());
		TrackListEntry::create(session, trackB.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool hasMore;
		const auto releases{trackList->getTopReleases({}, std::nullopt, hasMore)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0].id(), releaseB.getId());
		EXPECT_EQ(releases[1].id(), releaseA.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

 		bool hasMore;
		auto releases{trackList->getTopReleases({cluster1.getId()}, std::nullopt, hasMore)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases[0].id(), releaseA.getId());

		releases = trackList->getTopReleases({cluster2.getId()}, std::nullopt, hasMore);
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases[0].id(), releaseB.getId());

		releases = trackList->getTopReleases({cluster2.getId(), cluster1.getId()}, std::nullopt, hasMore);
		EXPECT_TRUE(releases.empty());

		releases = trackList->getTopReleases({cluster2.getId(), cluster3.getId()}, std::nullopt, hasMore);
		EXPECT_TRUE(releases.empty());
	}


	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, trackC.get(), trackList.get());
		TrackListEntry::create(session, trackC.get(), trackList.get());
		TrackListEntry::create(session, trackC.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

 		bool hasMore;
		auto releases {trackList->getTopReleases({cluster2.getId(), cluster3.getId()}, std::nullopt, hasMore)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases[0].id(), releaseC.getId());

		releases = trackList->getTopReleases({cluster2.getId()}, std::nullopt, hasMore);
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0].id(), releaseC.getId());
		EXPECT_EQ(releases[1].id(), releaseB.getId());
	}
}

TEST_F(DatabaseFixture,SingleTrackSingleReleaseSingleCluster)
{
	ScopedTrack track {session, "MyTrackFile"};
	ScopedRelease release {session, "MyRelease"};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType .lockAndGet(), "MyCluster"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Release::getAllIdsWithClusters(session).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track.get().modify()->setRelease(release.get());
		cluster.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto releases {Release::getAllIdsWithClusters(session)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front(), release.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(Cluster::getAllOrphans(session).empty());
		EXPECT_TRUE(Release::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {Release::getByClusters(session, {cluster.getId()})};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(cluster->getReleasesCount(), 1);
		EXPECT_EQ(cluster->getTracksCount(), 1);
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleArtistMultiClusters)
{
	ScopedTrack track {session, "MyTrackFile"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "Cluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "Cluster2"};
	ScopedCluster cluster3 {session, clusterType.lockAndGet(), "Cluster3"};
	{
		auto transaction {session.createUniqueTransaction()};

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist)};
		cluster1.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(ClusterType::getAllOrphans(session).empty());
		EXPECT_EQ(Cluster::getAllOrphans(session).size(), 2);
		EXPECT_TRUE(Release::getAllOrphans(session).empty());
		EXPECT_TRUE(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(track->getClusters().size(), 1);
		EXPECT_EQ(track->getClusterIds().size(), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster1.getId()}, Artist::SortMethod::ByName)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist.getId());

		EXPECT_TRUE(Artist::getByClusters(session, {cluster2.getId()}, Artist::SortMethod::ByName).empty());
		EXPECT_TRUE(Artist::getByClusters(session, {cluster3.getId()}, Artist::SortMethod::ByName).empty());

		cluster2.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster1.getId()}, Artist::SortMethod::ByName)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist.getId());

		artists = Artist::getByClusters(session, {cluster2.getId()}, Artist::SortMethod::ByName);
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist.getId());

		artists = Artist::getByClusters(session, {cluster1.getId(), cluster2.getId()}, Artist::SortMethod::ByName);
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist.getId());

		EXPECT_TRUE(Artist::getByClusters(session, {cluster3.getId()}, Artist::SortMethod::ByName).empty());
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleArtistMultiRolesMultiClusters)
{
	ScopedTrack track {session, "MyTrackFile"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::ReleaseArtist);
		cluster.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Cluster::getAllOrphans(session).empty());
		EXPECT_TRUE(Release::getAllOrphans(session).empty());
		EXPECT_TRUE(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster.getId()}, Artist::SortMethod::ByName)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist.getId());
	}
}

TEST_F(DatabaseFixture, MultiTracksSingleArtistMultiClusters)
{
	constexpr std::size_t nbTracks {10};
	constexpr std::size_t nbClusters {5};

	std::list<ScopedTrack> tracks;
	std::list<ScopedCluster> clusters;
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyType"};

	for (std::size_t i {}; i < nbClusters; ++i)
		clusters.emplace_back(session, clusterType.lockAndGet(), "MyCluster" + std::to_string(i));

	for (std::size_t i {}; i < nbTracks ; ++i)
	{
		tracks.emplace_back(session, "MyTrackFile" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLinkType::Artist);

		for (auto& cluster : clusters)
			cluster.get().modify()->addTrack(tracks.back().get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Cluster::getAllOrphans(session).empty());
		EXPECT_TRUE(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		std::set<IdType> clusterIds;
		std::transform(std::cbegin(clusters), std::cend(clusters), std::inserter(clusterIds, std::begin(clusterIds)), [](const ScopedCluster& cluster) { return cluster.getId(); });

		auto artists {Artist::getByClusters(session, clusterIds, Artist::SortMethod::ByName)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist.getId());
	}
}


TEST_F(DatabaseFixture, MultipleTracksSingleClusterSimilarity)
{
	std::list<ScopedTrack> tracks;
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyClusterType"};

	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		{
			auto transaction {session.createUniqueTransaction()};
			cluster.get().modify()->addTrack(tracks.back().get());
		}
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto similarTracks {Track::getSimilarTracks(session, {tracks.front().getId()})};
		EXPECT_EQ(similarTracks.size(), tracks.size() - 1);
		for (auto similarTrack : similarTracks)
		{
			EXPECT_TRUE(std::find_if(std::next(std::cbegin(tracks), 1), std::cend(tracks),
						[&](const ScopedTrack& track)
						{
							return similarTrack.id() == track.getId();
						}) != std::cend(tracks));
		}
	}
}

TEST_F(DatabaseFixture, MultipleTracksMultipleClustersSimilarity)
{
	std::list<ScopedTrack> tracks;
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	for (std::size_t i {}; i < 5; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		{
			auto transaction {session.createUniqueTransaction()};
			cluster1.get().modify()->addTrack(tracks.back().get());
		}
	}

	for (std::size_t i {5}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		{
			auto transaction {session.createUniqueTransaction()};
			cluster1.get().modify()->addTrack(tracks.back().get());
			cluster2.get().modify()->addTrack(tracks.back().get());
		}
	}

	{
		auto transaction {session.createSharedTransaction()};

		{
			auto similarTracks {Track::getSimilarTracks(session, {tracks.back().getId()}, 0, 4)};
			EXPECT_EQ(similarTracks.size(), 4);
			for (auto similarTrack : similarTracks)
				EXPECT_TRUE(std::find_if(std::next(std::cbegin(tracks), 5), std::next(std::cend(tracks), -1), [&](const ScopedTrack& track) { return similarTrack.id() == track.getId(); }) != std::cend(tracks));
		}

		{
			auto similarTracks {Track::getSimilarTracks(session, {tracks.front().getId()})};
			EXPECT_EQ(similarTracks.size(), tracks.size() - 1);
			for (auto similarTrack : similarTracks)
				EXPECT_TRUE(std::find_if(std::next(std::cbegin(tracks), 1), std::cend(tracks), [&](const ScopedTrack& track) { return similarTrack.id() == track.getId(); }) != std::cend(tracks));
		}
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtistSingleCluster)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedRelease release {session, "MyRelease"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Artist::getAllIdsWithClusters(session).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
		track.get().modify()->setRelease(release.get());
		cluster.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(Cluster::getAllOrphans(session).empty());
		EXPECT_TRUE(ClusterType::getAllOrphans(session).empty());
		EXPECT_TRUE(Artist::getAllOrphans(session).empty());
		EXPECT_TRUE(Release::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto artists {Artist::getAllIdsWithClusters(session)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front(), artist.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getByClusters(session, {cluster.getId()}, Artist::SortMethod::ByName)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist.getId());

		auto releases {artist->getReleases()};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release.getId());

		releases = artist->getReleases({cluster.getId()});
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtistMultiClusters)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedRelease release {session, "MyRelease"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	{
		auto transaction {session.createUniqueTransaction()};

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist)};
		track.get().modify()->setRelease(release.get());
		cluster1.get().modify()->addTrack(track.get());
		cluster2.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {artist->getReleases()};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release.getId());

		releases = artist->getReleases({cluster1.getId(), cluster2.getId()});
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackSingleCluster)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MyTrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};
	std::list<ScopedTrack> tracks;

	for (std::size_t i {}; i < 20; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

		if (i < 5)
			TrackListEntry::create(session, tracks.back().get(), trackList.get());

		if (i < 10)
			cluster.get().modify()->addTrack(tracks.back().get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto similarTracks {trackList->getSimilarTracks()};
		EXPECT_EQ(similarTracks.size(), 5);

		for (auto similarTrack : similarTracks)
			EXPECT_TRUE(std::any_of(std::next(std::cbegin(tracks), 5), std::cend(tracks), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack.id(); }));
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackMultiClusters)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MyTrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};
	std::list<ScopedTrack> tracks;

	for (std::size_t i {}; i < 20; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

		if (i < 5)
			TrackListEntry::create(session, tracks.back().get(), trackList.get());

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
		auto transaction {session.createSharedTransaction()};

		{
			const auto similarTracks {trackList->getSimilarTracks(0, 5)};
			ASSERT_EQ(similarTracks.size(), 5);

			for (auto similarTrack : similarTracks)
				EXPECT_TRUE(std::any_of(std::next(std::cbegin(tracks), 5), std::next(std::cbegin(tracks), 10), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack.id(); }));
		}

		{
			const auto similarTracks {trackList->getSimilarTracks(5, 10)};
			ASSERT_EQ(similarTracks.size(), 5);

			for (auto similarTrack : similarTracks)
				EXPECT_TRUE(std::any_of(std::next(std::cbegin(tracks), 10), std::next(std::cbegin(tracks), 15), [similarTrack](const ScopedTrack& track) { return track.getId() == similarTrack.id(); }));
		}

		EXPECT_TRUE(trackList->getSimilarTracks(10, 10).empty());
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackMultiClustersRecentlyPlayed)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MyTrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};
	ScopedCluster cluster3 {session, clusterType.lockAndGet(), "MyCluster3"};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack1"};
	ScopedArtist artist1 {session, "MyArtist1"};
	ScopedArtist artist2 {session, "MyArtist2"};
	ScopedRelease release1 {session, "MyRelease1"};
	ScopedRelease release2 {session, "MyRelease2"};

	const Wt::WDateTime now {Wt::WDateTime::currentDateTime()};

	{
		auto transaction {session.createUniqueTransaction()};

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
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		EXPECT_TRUE(trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults).empty());
		EXPECT_TRUE(trackList->getReleasesReverse({}, std::nullopt, moreResults).empty());
		EXPECT_TRUE(trackList->getTracksReverse({}, std::nullopt, moreResults).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track1.get(), trackList.get(), now);
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size() , 1);
		EXPECT_EQ(releases.front().id(), release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front().id(), track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster1.getId()}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({cluster1.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release1.getId());

		const auto tracks {trackList->getTracksReverse({cluster1.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front().id(), track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster3.getId()}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({cluster3.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release1.getId());

		const auto tracks {trackList->getTracksReverse({cluster3.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front().id(), track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster1.getId()}, TrackArtistLinkType::Artist, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, TrackArtistLinkType::Artist, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		EXPECT_TRUE(trackList->getArtistsReverse({cluster2.getId()}, std::nullopt, std::nullopt, moreResults).empty());
		EXPECT_TRUE(trackList->getReleasesReverse({cluster2.getId()}, std::nullopt, moreResults).empty());
		EXPECT_TRUE(trackList->getTracksReverse({cluster2.getId()}, std::nullopt, moreResults).empty());

		EXPECT_TRUE(trackList->getArtistsReverse({}, TrackArtistLinkType::ReleaseArtist, std::nullopt, moreResults).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track2.get(), trackList.get(), now.addSecs(1));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_EQ(artists[0].id(), artist2.getId());
		EXPECT_EQ(artists[1].id(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0].id(), release2.getId());
		EXPECT_EQ(releases[1].id(), release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0].id(), track2.getId());
		EXPECT_EQ(tracks[1].id(),track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster3.getId()}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_EQ(artists[0].id(), artist2.getId());
		EXPECT_EQ(artists[1].id(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({cluster3.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0].id(), release2.getId());
		EXPECT_EQ(releases[1].id(), release1.getId());

		const auto tracks {trackList->getTracksReverse({cluster3.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0].id(), track2.getId());
		EXPECT_EQ(tracks[1].id(), track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster1.getId()}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({cluster1.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release1.getId());

		const auto tracks {trackList->getTracksReverse({cluster1.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front().id(), track1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster2.getId()}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front().id(), artist2.getId());

		const auto releases {trackList->getReleasesReverse({cluster2.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release2.getId());

		const auto tracks {trackList->getTracksReverse({cluster2.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front().id(), track2.getId());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackListEntry::create(session, track1.get(), trackList.get(), now.addSecs(2));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_EQ(artists[0].id(), artist1.getId());
		EXPECT_EQ(artists[1].id(), artist2.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0].id(), release1.getId());
		EXPECT_EQ(releases[1].id(), release2.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0].id(), track1.getId());
		EXPECT_EQ(tracks[1].id(), track2.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsReverse({cluster3.getId()}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_EQ(artists[0].id(), artist1.getId());
		EXPECT_EQ(artists[1].id(), artist2.getId());

		const auto releases {trackList->getReleasesReverse({cluster3.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0].id(), release1.getId());
		EXPECT_EQ(releases[1].id(), release2.getId());

		const auto tracks {trackList->getTracksReverse({cluster3.getId()}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0].id(), track1.getId());
		EXPECT_EQ(tracks[1].id(), track2.getId());
	}
}

TEST_F(DatabaseFixture, MultipleTracksMultipleArtistsMultiClusters)
{
	ScopedArtist artist1 {session, "MyArtist1"};
	ScopedArtist artist2 {session, "MyArtist2"};
	ScopedArtist artist3 {session, "MyArtist3"};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(artist1->getSimilarArtists().empty());
		EXPECT_TRUE(artist2->getSimilarArtists().empty());
		EXPECT_TRUE(artist3->getSimilarArtists().empty());
	}

	std::list<ScopedTrack> tracks;
	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

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
		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, tracks.back().get(), artist3.get(), TrackArtistLinkType::Artist);
		cluster2.get().modify()->addTrack(tracks.back().get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		{
			auto artists {artist1->getSimilarArtists()};
			ASSERT_EQ(artists.size(), 1);
			EXPECT_EQ(artists.front().id(), artist2.getId());
		}

		{
			auto artists {artist1->getSimilarArtists({TrackArtistLinkType::Artist})};
			ASSERT_EQ(artists.size(), 1);
			EXPECT_EQ(artists.front().id(), artist2.getId());
		}

		{
			auto artists {artist1->getSimilarArtists({TrackArtistLinkType::ReleaseArtist})};
			EXPECT_EQ(artists.empty(), 1);
		}

		{
			auto artists {artist1->getSimilarArtists({TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist})};
			ASSERT_EQ(artists.size(), 1);
			EXPECT_EQ(artists.front().id(), artist2.getId());
		}

		{
			auto artists {artist1->getSimilarArtists({TrackArtistLinkType::Composer})};
			EXPECT_TRUE(artists.empty());
		}

		{
			auto artists {artist2->getSimilarArtists()};
			ASSERT_EQ(artists.size(), 2);
			EXPECT_EQ(artists[0].id(), artist1.getId());
			EXPECT_EQ(artists[1].id(), artist3.getId());
		}
	}
}

TEST_F(DatabaseFixture, MultipleTracksMultipleReleasesMultiClusters)
{
	ScopedRelease release1 {session, "MyRelease1"};
	ScopedRelease release2 {session, "MyRelease2"};
	ScopedRelease release3 {session, "MyRelease3"};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster1 {session, clusterType.lockAndGet(), "MyCluster1"};
	ScopedCluster cluster2 {session, clusterType.lockAndGet(), "MyCluster2"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(release1->getSimilarReleases().empty());
		EXPECT_TRUE(release2->getSimilarReleases().empty());
		EXPECT_TRUE(release3->getSimilarReleases().empty());
	}

	std::list<ScopedTrack> tracks;
	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

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
		auto transaction {session.createUniqueTransaction()};
		tracks.back().get().modify()->setRelease(release3.get());
		cluster2.get().modify()->addTrack(tracks.back().get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		{
			auto releases {release1->getSimilarReleases()};
			ASSERT_EQ(releases.size(), 1);
			EXPECT_EQ(releases.front().id(), release2.getId());
		}

		{
			auto releases {release2->getSimilarReleases()};
			ASSERT_EQ(releases.size(), 2);
			EXPECT_EQ(releases[0].id(), release1.getId());
			EXPECT_EQ(releases[1].id(), release3.getId());
		}
	}
}


