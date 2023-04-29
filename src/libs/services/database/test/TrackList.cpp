/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <list>

#include "Common.hpp"

using namespace Database;

TEST_F(DatabaseFixture, SingleTrackList)
{
	ScopedUser user {session, "MyUser"};
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(TrackList::getCount(session), 0);
	}

	ScopedTrackList trackList {session, "MytrackList", TrackListType::Playlist, false, user.lockAndGet()};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(TrackList::getCount(session), 1);
	}
}

TEST_F(DatabaseFixture, SingleTrackListSingleTrack)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList1 {session, "MyTrackList1", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrackList trackList2 {session, "MyTrackList2", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrack track {session, "MyTrack"};

	{
		auto transaction {session.createSharedTransaction()};

		auto tracks {Track::find(session, Track::FindParameters {}.setTrackList(trackList1.getId()))};
		EXPECT_EQ(tracks.results.size(), 0);

		tracks = Track::find(session, Track::FindParameters {}.setTrackList(trackList2.getId()));
		EXPECT_EQ(tracks.results.size(), 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		session.create<TrackListEntry>(track.get(), trackList1.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto tracks {Track::find(session, Track::FindParameters {}.setTrackList(trackList1.getId()))};
		ASSERT_EQ(tracks.results.size(), 1);
		EXPECT_EQ(tracks.results.front(), track.getId());

		tracks = Track::find(session, Track::FindParameters {}.setTrackList(trackList2.getId()));
		EXPECT_EQ(tracks.results.size(), 0);
	}
}

TEST_F(DatabaseFixture, TrackList_SortMethod)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList2 {session, "MyTrackList2", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrackList trackList1 {session, "MyTrackList1", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrack track {session, "MyTrack"};

	{
		auto transaction {session.createSharedTransaction()};

		const auto trackLists {TrackList::find(session, TrackList::FindParameters {}.setSortMethod(TrackListSortMethod::Name))};
		ASSERT_EQ(trackLists.results.size(), 2);
		EXPECT_EQ(trackLists.results[0], trackList1.getId());
		EXPECT_EQ(trackLists.results[1], trackList2.getId());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		trackList1.get().modify()->setLastModifiedDateTime(Wt::WDateTime {Wt::WDate {1900,1,1}});
		trackList2.get().modify()->setLastModifiedDateTime(Wt::WDateTime {Wt::WDate {1900,1,2}});
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto trackLists {TrackList::find(session, TrackList::FindParameters {}.setSortMethod(TrackListSortMethod::LastModifiedDesc))};
		ASSERT_EQ(trackLists.results.size(), 2);
		EXPECT_EQ(trackLists.results[0], trackList2.getId());
		EXPECT_EQ(trackLists.results[1], trackList1.getId());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		trackList1.get().modify()->setLastModifiedDateTime(Wt::WDateTime {Wt::WDate {1900,1,2}});
		trackList2.get().modify()->setLastModifiedDateTime(Wt::WDateTime {Wt::WDate {1900,1,1}});
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto trackLists {TrackList::find(session, TrackList::FindParameters {}.setSortMethod(TrackListSortMethod::LastModifiedDesc))};
		ASSERT_EQ(trackLists.results.size(), 2);
		EXPECT_EQ(trackLists.results[0], trackList1.getId());
		EXPECT_EQ(trackLists.results[1], trackList2.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrack)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackListType::Playlist, false, user.lockAndGet()};
	std::list<ScopedTrack> tracks;

	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};
		session.create<TrackListEntry>(tracks.back().get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_EQ(trackList->getCount(), tracks.size());
		const auto trackIds {trackList->getTrackIds()};
		ASSERT_EQ(trackIds.size(), tracks.size());

		// Same order
		std::size_t i {};
		for (const ScopedTrack& track : tracks)
			EXPECT_EQ(track.getId(), trackIds[i++]);
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto reverseTracks {trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults)};
		ASSERT_EQ(reverseTracks.size(), tracks.size());

		// Reverse order
		ASSERT_TRUE(tracks.size() > 0);
		std::size_t i {tracks.size() - 1};
		for (const ScopedTrack& track : tracks)
			EXPECT_EQ(track.getId(), reverseTracks[i--]->getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackDateTime)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack2"};
	ScopedTrack track3 {session, "MyTrack3"};

	{
		Wt::WDateTime now {Wt::WDateTime::currentDateTime()};
		auto transaction {session.createUniqueTransaction()};
		session.create<TrackListEntry>(track1.get(), trackList.get(), now);
		session.create<TrackListEntry>(track2.get(), trackList.get(), now.addSecs(-1));
		session.create<TrackListEntry>(track3.get(), trackList.get(), now.addSecs(1));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults;
		const auto tracks {trackList.get()->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 3);
		EXPECT_EQ(tracks.front()->getId(), track3.getId());
		EXPECT_EQ(tracks.back()->getId(), track2.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackRecentlyPlayed)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MyTrackList", TrackListType::Playlist, false, user.lockAndGet()};
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
	}
	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		EXPECT_TRUE(trackList->getArtistsOrderedByRecentFirst({}, std::nullopt, std::nullopt, moreResults).empty());
		EXPECT_TRUE(trackList->getReleasesOrderedByRecentFirst({}, std::nullopt, moreResults).empty());
		EXPECT_TRUE(trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults).empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		session.create<TrackListEntry>(track1.get(), trackList.get(), now);
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsOrderedByRecentFirst({}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist1.getId());

		const auto releases {trackList->getReleasesOrderedByRecentFirst({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front()->getId(), release1.getId());

		const auto tracks {trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults)};
		EXPECT_EQ(tracks.size(), 1);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		session.create<TrackListEntry>(track2.get(), trackList.get(), now.addSecs(1));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsOrderedByRecentFirst({}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_EQ(artists[0]->getId(), artist2.getId());
		EXPECT_EQ(artists[1]->getId(), artist1.getId());

		const auto releases {trackList->getReleasesOrderedByRecentFirst({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0]->getId(), release2.getId());
		EXPECT_EQ(releases[1]->getId(), release1.getId());

		const auto tracks {trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0]->getId(), track2.getId());
		EXPECT_EQ(tracks[1]->getId(), track1.getId());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		session.create<TrackListEntry>(track1.get(), trackList.get(), now.addSecs(2));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtistsOrderedByRecentFirst({}, std::nullopt, std::nullopt, moreResults)};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_EQ(artists[0]->getId(), artist1.getId());
		EXPECT_EQ(artists[1]->getId(), artist2.getId());

		const auto releases {trackList->getReleasesOrderedByRecentFirst({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0]->getId(), release1.getId());
		EXPECT_EQ(releases[1]->getId(), release2.getId());

		const auto tracks {trackList->getTracksOrderedByRecentFirst({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0]->getId(), track1.getId());
		EXPECT_EQ(tracks[1]->getId(), track2.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackList_getArtists)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack2"};
	ScopedRelease release {session, "MyRelease"};
	ScopedArtist artist1 {session, "MyArtist1"};
	ScopedArtist artist2 {session, "MyArtist2"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(trackList->getCount(), 0);
		bool moreResults {};
		const auto artists {trackList->getArtists({} /*clusters*/, std::nullopt /* linkType */, ArtistSortMethod::ByName, std::nullopt /* range */, moreResults)};
		ASSERT_TRUE(artists.empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, track1.get(), artist1.get(), TrackArtistLinkType::Artist);
		session.create<TrackListEntry>(track1.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(trackList->getCount(), 1);
		bool moreResults {};
		const auto artists {trackList->getArtists({} /*clusters*/, std::nullopt /* linkType */, ArtistSortMethod::ByName, std::nullopt /* range */, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtists({} /*clusters*/, TrackArtistLinkType::ReleaseArtist, ArtistSortMethod::ByName, std::nullopt /* range */, moreResults)};
		EXPECT_TRUE(artists.empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtists({} /*clusters*/, TrackArtistLinkType::Artist, ArtistSortMethod::ByName, std::nullopt /* range */, moreResults)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist1.getId());
	}

	{
		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, track2.get(), artist2.get(), TrackArtistLinkType::Artist);
		session.create<TrackListEntry>(track2.get(), trackList.get());
		artist1.get().modify()->setSortName("ZZZ");
		artist2.get().modify()->setSortName("AAA");
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtists({} /*clusters*/, TrackArtistLinkType::Artist, ArtistSortMethod::ByName, std::nullopt /* range */, moreResults)};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_EQ(artists.front()->getId(), artist1.getId());
		EXPECT_EQ(artists.back()->getId(), artist2.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults {};
		const auto artists {trackList->getArtists({} /*clusters*/, TrackArtistLinkType::Artist, ArtistSortMethod::BySortName, std::nullopt /* range */, moreResults)};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_EQ(artists.front()->getId(), artist2.getId());
		EXPECT_EQ(artists.back()->getId(), artist1.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackList_getReleases)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrack track {session, "MyTrack"};
	ScopedRelease release {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(trackList->getCount(), 0);
		bool moreResults {};
		const auto releases {trackList->getReleases({} /*clusters*/, std::nullopt /* range */, moreResults)};
		ASSERT_TRUE(releases.empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track.get().modify()->setRelease(release.get());

		session.create<TrackListEntry>(track.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_EQ(trackList->getCount(), 1);
		bool moreResults {};
		const auto releases {trackList->getReleases({} /*clusters*/, std::nullopt /* range */, moreResults)};
		ASSERT_EQ(releases.size(), 1);
		ASSERT_EQ(releases.front()->getId(), release->getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackList_getTracks)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack1"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(trackList->getCount(), 0);
		bool moreResults {};
		const auto tracks {trackList->getTracks({} /*clusters*/, std::nullopt /* range */, moreResults)};
		ASSERT_TRUE(tracks.empty());
	}

	{
		auto transaction {session.createUniqueTransaction()};
		session.create<TrackListEntry>(track1.get(), trackList.get());
		session.create<TrackListEntry>(track2.get(), trackList.get());
		session.create<TrackListEntry>(track1.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_EQ(trackList->getCount(), 3);
		bool moreResults {};
		const auto tracks {trackList->getTracks({} /*clusters*/, std::nullopt /* range */, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		ASSERT_EQ(tracks[0]->getId(), track1->getId());
		ASSERT_EQ(tracks[1]->getId(), track2->getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackListSingleTrackWithCluster)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList1 {session, "MyTrackList1", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrackList trackList2 {session, "MyTrackList2", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedClusterType clusterType {session, "MyClusterType"};
	ScopedCluster cluster {session, clusterType.lockAndGet(), "MyCluster"};
	ScopedTrack track {session, "MyTrack"};

	{
		auto transaction {session.createSharedTransaction()};

		auto trackLists {TrackList::find(session, TrackList::FindParameters {}.setClusters({cluster.getId()}))};
		EXPECT_EQ(trackLists.results.size(), 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		session.create<TrackListEntry>(track.get(), trackList1.get());
		cluster.get().modify()->addTrack(track.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto trackLists {TrackList::find(session, TrackList::FindParameters {}.setClusters({cluster.getId()}))};
		ASSERT_EQ(trackLists.results.size(), 1);
		EXPECT_EQ(trackLists.results.front(), trackList1.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackList_getEntries)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MyTrackList", TrackListType::Playlist, false, user.lockAndGet()};
	ScopedTrack track1 {session, "MyTrack"};
	ScopedTrack track2 {session, "MyTrack"};

	{
		auto transaction {session.createUniqueTransaction()};
		session.create<TrackListEntry>(track1.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto entries {trackList.get()->getEntries()};
		ASSERT_EQ(entries.size(), 1);
		EXPECT_EQ(entries.front()->getTrack()->getId(), track1.getId());
	}

	{
		auto transaction {session.createUniqueTransaction()};
		session.create<TrackListEntry>(track2.get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto entries {trackList.get()->getEntries()};
		ASSERT_EQ(entries.size(), 2);
		EXPECT_EQ(entries[0]->getTrack()->getId(), track1.getId());
		EXPECT_EQ(entries[1]->getTrack()->getId(), track2.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto entries {trackList.get()->getEntries(Range {1, 1})};
		ASSERT_EQ(entries.size(), 1);
		EXPECT_EQ(entries[0]->getTrack()->getId(), track2.getId());
	}
}
