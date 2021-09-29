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

TEST_F(DatabaseFixture, MultiTracksSingleArtistSingleRelease)
{
	constexpr std::size_t nbTracks {10};
	std::list<ScopedTrack> tracks;
	ScopedArtist artist {session, "MyArtst"};
	ScopedRelease release {session, "MyRelease"};

	for (std::size_t i {}; i < nbTracks; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, tracks.back().get(), artist.get(), TrackArtistLinkType::Artist);
		tracks.back().get().modify()->setRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Release::getAllOrphans(session).empty());
		EXPECT_TRUE(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(artist->getReleaseCount(), 1);
		ASSERT_EQ(artist->getReleases().size(), 1);
		EXPECT_EQ(artist->getReleases().front()->getId(), release.getId());

		EXPECT_EQ(release->getTracks().size(), nbTracks);
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleReleaseSingleArtist)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedRelease release {session, "MyRelease"};
	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createUniqueTransaction()};

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist)};
		track.get().modify()->setRelease(release.get());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto releases {artist->getReleases()};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front()->getId(), release.getId());

		EXPECT_EQ(artist->getReleaseCount(), 1);

		auto artists {release->getArtists()};
		ASSERT_EQ(artists.size(), 1);
		ASSERT_EQ(artists.front()->getId(), artist.getId());
	}
}

TEST_F(DatabaseFixture, SingleUser)
{
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(User::getAll(session).empty());
		EXPECT_TRUE(User::getAllIds(session).empty());
	}

	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(user->getQueuedTrackList(session)->getCount(), 0);
		EXPECT_EQ(User::getAll(session).size(), 1);
		EXPECT_EQ(User::getAllIds(session).size(), 1);
	}
}

TEST_F(DatabaseFixture, SingleStarredArtist)
{
	ScopedArtist artist {session, "MyArtist"};
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createUniqueTransaction()};

		EXPECT_FALSE(user->hasStarredArtist(artist.get()));
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto trackArtistLink {TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist)};
		user.get().modify()->starArtist(artist.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(user->hasStarredArtist(artist.get()));

		bool hasMore {};
		auto artists {Artist::getStarred(session, user.get(), {}, std::nullopt, Artist::SortMethod::BySortName, std::nullopt, hasMore)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist.getId());
		EXPECT_FALSE(hasMore);
	}
}

TEST_F(DatabaseFixture, SingleStarredRelease)
{
	ScopedRelease release {session, "MyRelease"};
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(user->hasStarredRelease(release.get()));
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track.get().modify()->setRelease(release.get());
		user.get().modify()->starRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(user->hasStarredRelease(release.get()));

		bool hasMore {};
		auto releases {Release::getStarred(session, user.get(), {}, std::nullopt, hasMore)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front()->getId(), release.getId());
		EXPECT_FALSE(hasMore);
	}
}

TEST_F(DatabaseFixture, SingleStarredTrack)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};

	{
		auto transaction {session.createUniqueTransaction()};

		EXPECT_FALSE(user->hasStarredTrack(track.get()));
	}

	{
		auto transaction {session.createUniqueTransaction()};

		user.get().modify()->starTrack(track.get());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		EXPECT_TRUE(user->hasStarredTrack(track.get()));

		bool hasMore {};
		auto tracks {Track::getStarred(session, user.get(), {}, std::nullopt, hasMore)};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front()->getId(), track.getId());
		EXPECT_FALSE(hasMore);
	}
}

TEST_F(DatabaseFixture, SingleTrackList)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackList::Type::Playlist, false, user.lockAndGet()};

	{
		auto transaction {session.createSharedTransaction()};

		auto trackLists {TrackList::getAll(session, user.get(), TrackList::Type::Playlist)};
		ASSERT_EQ(trackLists.size(), 1);
		EXPECT_EQ(trackLists.front()->getId(), trackList.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrack)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	std::list<ScopedTrack> tracks;

	for (std::size_t i {}; i < 10; ++i)
	{
		tracks.emplace_back(session, "MyTrack" + std::to_string(i));

		auto transaction {session.createUniqueTransaction()};
		TrackListEntry::create(session, tracks.back().get(), trackList.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_EQ(trackList->getCount(), tracks.size());
		const auto trackIds {trackList->getTrackIds()};
		for (auto trackId : trackIds)
			EXPECT_TRUE(std::any_of(std::cbegin(tracks), std::cend(tracks), [trackId](const ScopedTrack& track) { return track.getId() == trackId; }));
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackDateTime)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MytrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack2"};
	ScopedTrack track3 {session, "MyTrack3"};

	{
		Wt::WDateTime now {Wt::WDateTime::currentDateTime()};
		auto transaction {session.createUniqueTransaction()};
		TrackListEntry::create(session, track1.get(), trackList.get(), now);
		TrackListEntry::create(session, track2.get(), trackList.get(), now.addSecs(-1));
		TrackListEntry::create(session, track3.get(), trackList.get(), now.addSecs(1));
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults;
		const auto tracks {trackList.get()->getTracksReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 3);
		EXPECT_EQ(tracks.front()->getId(), track3.getId());
		EXPECT_EQ(tracks.back()->getId(), track2.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackListMultipleTrackRecentlyPlayed)
{
	ScopedUser user {session, "MyUser"};
	ScopedTrackList trackList {session, "MyTrackList", TrackList::Type::Playlist, false, user.lockAndGet()};
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
		EXPECT_EQ(artists.front()->getId(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front()->getId(), release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		EXPECT_EQ(tracks.size(), 1);
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
		EXPECT_EQ(artists[0]->getId(), artist2.getId());
		EXPECT_EQ(artists[1]->getId(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0]->getId(), release2.getId());
		EXPECT_EQ(releases[1]->getId(), release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0]->getId(), track2.getId());
		EXPECT_EQ(tracks[1]->getId(), track1.getId());
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
		EXPECT_EQ(artists[0]->getId(), artist1.getId());
		EXPECT_EQ(artists[1]->getId(), artist2.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0]->getId(), release1.getId());
		EXPECT_EQ(releases[1]->getId(), release2.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0]->getId(), track1.getId());
		EXPECT_EQ(tracks[1]->getId(), track2.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleUserSingleBookmark)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedUser user {session, "MyUser"};
	ScopedTrackBookmark bookmark {session, user.lockAndGet(), track.lockAndGet()};

	{
		auto transaction {session.createUniqueTransaction()};

		bookmark.get().modify()->setComment("MyComment");
		bookmark.get().modify()->setOffset(std::chrono::milliseconds {5});
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(TrackBookmark::getAll(session).size(), 1);

		const auto bookmarks {TrackBookmark::getByUser(session, user.get())};
		ASSERT_EQ(bookmarks.size(), 1);
		EXPECT_EQ(bookmarks.back(), bookmark.get());
	}
	{
		auto transaction {session.createSharedTransaction()};

		auto userBookmark {TrackBookmark::getByUser(session, user.get(), track.get())};
		ASSERT_TRUE(userBookmark);
		EXPECT_EQ(userBookmark, bookmark.get());

		EXPECT_EQ(userBookmark->getOffset(), std::chrono::milliseconds {5});
		EXPECT_EQ(userBookmark->getComment(), "MyComment");
	}
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
