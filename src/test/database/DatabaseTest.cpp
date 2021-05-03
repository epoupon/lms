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

TEST_F(DatabaseFixture, SingleRelease)
{
	ScopedRelease release {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {Release::getAllOrphans(session)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release.getId());

		releases = Release::getAll(session);
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release.getId());
		EXPECT_EQ(release->getDuration(), std::chrono::seconds {0});
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleRelease)
{
	ScopedRelease release {session, "MyRelease"};

	{
		ScopedTrack track {session, "MyTrack"};
		{
			auto transaction {session.createUniqueTransaction()};

			track.get().modify()->setRelease(release.get());
			track.get().modify()->setName("MyTrackName");
			release.get().modify()->setName("MyReleaseName");
		}

		{
			auto transaction {session.createSharedTransaction()};
			EXPECT_TRUE(Release::getAllOrphans(session).empty());

			EXPECT_EQ(release->getTracksCount(), 1);
			ASSERT_EQ(release->getTracks().size(), 1);
			EXPECT_EQ(release->getTracks().front().id(), track.getId());
		}

		{
			auto transaction {session.createUniqueTransaction()};

			ASSERT_TRUE(track->getRelease());
			EXPECT_EQ(track->getRelease().id(), release.getId());
		}

		{
			auto transaction {session.createUniqueTransaction()};
			auto tracks {Track::getByNameAndReleaseName(session, "MyTrackName", "MyReleaseName")};
			ASSERT_EQ(tracks.size(), 1);
			EXPECT_EQ(tracks.front().id(), track.getId());
		}
		{
			auto transaction {session.createUniqueTransaction()};
			auto tracks {Track::getByNameAndReleaseName(session, "MyTrackName", "MyReleaseFoo")};
			EXPECT_EQ(tracks.size(), 0);
		}
		{
			auto transaction {session.createUniqueTransaction()};
			auto tracks {Track::getByNameAndReleaseName(session, "MyTrackFoo", "MyReleaseName")};
			EXPECT_EQ(tracks.size(), 0);
		}
	}

	{
		auto transaction {session.createUniqueTransaction()};

		EXPECT_TRUE(release->getTracks().empty());

		auto releases {Release::getAllOrphans(session)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release.getId());
	}
}

TEST_F(DatabaseFixture, MulitpleReleaseSearchByName)
{
	ScopedRelease release1 {session, "MyRelease"};
	ScopedRelease release2 {session, "MyRelease%"};
	ScopedRelease release3 {session, "%MyRelease"};
	ScopedRelease release4 {session, "MyRelease%Foo"};
	ScopedRelease release5 {session, "Foo%MyRelease"};
	ScopedRelease release6 {session, "_yRelease"};

	// filters does not work on orphans
	ScopedTrack track1 {session, "MyTrack"};
	ScopedTrack track2 {session, "MyTrack"};
	ScopedTrack track3 {session, "MyTrack"};
	ScopedTrack track4 {session, "MyTrack"};
	ScopedTrack track5 {session, "MyTrack"};
	ScopedTrack track6 {session, "MyTrack"};

	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setRelease(release1.get());
		track2.get().modify()->setRelease(release2.get());
		track3.get().modify()->setRelease(release3.get());
		track4.get().modify()->setRelease(release4.get());
		track5.get().modify()->setRelease(release5.get());
		track6.get().modify()->setRelease(release6.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool more;
		{
			const auto releases {Release::getByFilter(session, {}, {"Release"}, std::nullopt, more)};
			EXPECT_EQ(releases.size(), 6);
		}

		{
			const auto releases {Release::getByFilter(session, {}, {"MyRelease"}, std::nullopt, more)};
			EXPECT_EQ(releases.size(), 5);
			EXPECT_TRUE(std::none_of(std::cbegin(releases), std::cend(releases), [&](const Release::pointer& release) { return release.id() == release6.getId(); }));
		}
		{
			const auto releases {Release::getByFilter(session, {}, {"MyRelease%"}, std::nullopt, more)};
			EXPECT_EQ(releases.size(), 2);
			EXPECT_EQ(releases[0].id(), release2.getId());
			EXPECT_EQ(releases[1].id(), release4.getId());
		}
		{
			const auto releases {Release::getByFilter(session, {}, {"%MyRelease"}, std::nullopt, more)};
			EXPECT_EQ(releases.size(), 2);
			EXPECT_EQ(releases[0].id(), release3.getId());
			EXPECT_EQ(releases[1].id(), release5.getId());
		}
		{
			const auto releases {Release::getByFilter(session, {}, {"Foo%MyRelease"}, std::nullopt, more)};
			EXPECT_EQ(releases.size(), 1);
			EXPECT_EQ(releases[0].id(), release5.getId());
		}
		{
			const auto releases {Release::getByFilter(session, {}, {"MyRelease%Foo"}, std::nullopt, more)};
			EXPECT_EQ(releases.size(), 1);
			EXPECT_EQ(releases[0].id(), release4.getId());
		}
	}
}

TEST_F(DatabaseFixture, MultiTracksSingleReleaseTotalDiscTrack)
{
	ScopedRelease release1 {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(release1->getTotalTrack());
		EXPECT_FALSE(release1->getTotalDisc());
	}

	ScopedTrack track1 {session, "MyTrack"};
	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setRelease(release1.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(release1->getTotalTrack());
		EXPECT_FALSE(release1->getTotalDisc());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setTotalTrack(36);
		track1.get().modify()->setTotalDisc(6);
	}

	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_TRUE(release1->getTotalTrack());
		EXPECT_EQ(*release1->getTotalTrack(), 36);
		ASSERT_TRUE(release1->getTotalDisc());
		EXPECT_EQ(*release1->getTotalDisc(), 6);
	}

	ScopedTrack track2 {session, "MyTrack2"};
	{
		auto transaction {session.createUniqueTransaction()};

		track2.get().modify()->setRelease(release1.get());
		track2.get().modify()->setTotalTrack(37);
		track2.get().modify()->setTotalDisc(67);
	}

	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_TRUE(release1->getTotalTrack());
		EXPECT_EQ(*release1->getTotalTrack(), 37);
		ASSERT_TRUE(release1->getTotalDisc());
		EXPECT_EQ(*release1->getTotalDisc(), 67);
	}

	ScopedRelease release2 {session, "MyRelease2"};
	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(release2->getTotalTrack());
		EXPECT_FALSE(release2->getTotalDisc());
	}

	ScopedTrack track3 {session, "MyTrack3"};
	{
		auto transaction {session.createUniqueTransaction()};

		track3.get().modify()->setRelease(release2.get());
		track3.get().modify()->setTotalTrack(7);
		track3.get().modify()->setTotalDisc(5);
	}
	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_TRUE(release1->getTotalTrack());
		EXPECT_EQ(*release1->getTotalTrack(), 37);
		ASSERT_TRUE(release1->getTotalDisc());
		EXPECT_EQ(*release1->getTotalDisc(), 67);
		ASSERT_TRUE(release2->getTotalTrack());
		EXPECT_EQ(*release2->getTotalTrack(), 7);
		ASSERT_TRUE(release2->getTotalDisc());
		EXPECT_EQ(*release2->getTotalDisc(), 5);
	}
}

TEST_F(DatabaseFixture, MultiTracksSingleReleaseFirstTrack)
{
	ScopedRelease release1 {session, "MyRelease1"};
	ScopedRelease release2 {session, "MyRelease2"};

	ScopedTrack track1A {session, "MyTrack1A"};
	ScopedTrack track1B {session, "MyTrack1B"};
	ScopedTrack track2A {session, "MyTrack2A"};
	ScopedTrack track2B {session, "MyTrack2B"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(release1->getFirstTrack());
		EXPECT_FALSE(release2->getFirstTrack());
	}

	{
		auto transaction {session.createUniqueTransaction()};

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
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(release1->getFirstTrack());
		EXPECT_TRUE(release2->getFirstTrack());

		EXPECT_EQ(release1->getFirstTrack().id(), track1A.getId());
		EXPECT_EQ(release2->getFirstTrack().id(), track2B.getId());
	}
}

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
		EXPECT_EQ(artist->getReleases().front().id(), release.getId());

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
		EXPECT_EQ(releases.front().id(), release.getId());

		EXPECT_EQ(artist->getReleaseCount(), 1);

		auto artists {release->getArtists()};
		ASSERT_EQ(artists.size(), 1);
		ASSERT_EQ(artists.front().id(), artist.getId());
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
		EXPECT_EQ(artists.front().id(), artist.getId());
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
		EXPECT_EQ(releases.front().id(), release.getId());
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
		EXPECT_EQ(tracks.front().id(), track.getId());
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
		EXPECT_EQ(trackLists.front().id(), trackList.getId());
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
		EXPECT_EQ(tracks.front().id(), track3.getId());
		EXPECT_EQ(tracks.back().id(), track2.getId());
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
		EXPECT_EQ(artists.front().id(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front().id(), release1.getId());

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
		EXPECT_EQ(artists[0].id(), artist2.getId());
		EXPECT_EQ(artists[1].id(), artist1.getId());

		const auto releases {trackList->getReleasesReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(releases.size(), 2);
		EXPECT_EQ(releases[0].id(), release2.getId());
		EXPECT_EQ(releases[1].id(), release1.getId());

		const auto tracks {trackList->getTracksReverse({}, std::nullopt, moreResults)};
		ASSERT_EQ(tracks.size(), 2);
		EXPECT_EQ(tracks[0].id(), track2.getId());
		EXPECT_EQ(tracks[1].id(), track1.getId());
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
