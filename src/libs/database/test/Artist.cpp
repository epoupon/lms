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

using namespace Database;

TEST_F(DatabaseFixture, SingleArtist)
{
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_FALSE(Artist::exists(session, 35));
		EXPECT_FALSE(Artist::exists(session, 0));
		EXPECT_FALSE(Artist::exists(session, 1));
	}

	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(artist.get());
		EXPECT_FALSE(!artist.get());
		EXPECT_EQ(artist.get()->getId(), artist.getId());

		EXPECT_TRUE(Artist::exists(session, artist.getId()));
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::getAll(session, Artist::SortMethod::ByName)};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist.getId());

		artists = Artist::getAllOrphans(session);
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist.getId());
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleArtist)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist.getId());

		EXPECT_EQ(artist->getReleaseCount(), 0);

		ASSERT_EQ(track->getArtistLinks().size(), 1);
		auto artistLink {track->getArtistLinks().front()};
		EXPECT_EQ(artistLink->getTrack()->getId(), track.getId());
		EXPECT_EQ(artistLink->getArtist()->getId(), artist.getId());

		ASSERT_EQ(track->getArtists({TrackArtistLinkType::Artist}).size(), 1);
		EXPECT_TRUE(track->getArtists({TrackArtistLinkType::ReleaseArtist}).empty());
		EXPECT_EQ(track->getArtists({}).size(), 1);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		auto tracks {artist->getTracks()};
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front()->getId(), track.getId());

		EXPECT_TRUE(artist->getTracks(TrackArtistLinkType::ReleaseArtist).empty());
		EXPECT_EQ(artist->getTracks(TrackArtistLinkType::Artist).size(), 1);
	}
}

TEST_F(DatabaseFixture, SingleTrackSingleArtistMultiRoles)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedArtist artist {session, "MyArtist"};
	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::ReleaseArtist);
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Writer);
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};
		bool hasMore{};
		EXPECT_EQ(Artist::getByFilter(session, {}, {}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, hasMore).size(), 1);
		EXPECT_EQ(Artist::getByFilter(session, {}, {}, TrackArtistLinkType::Artist, Artist::SortMethod::ByName, std::nullopt, hasMore).size(), 1);
		EXPECT_EQ(Artist::getByFilter(session, {}, {}, TrackArtistLinkType::ReleaseArtist, Artist::SortMethod::ByName, std::nullopt, hasMore).size(), 1);
		EXPECT_EQ(Artist::getByFilter(session, {}, {}, TrackArtistLinkType::Writer, Artist::SortMethod::ByName, std::nullopt, hasMore).size(), 1);
		EXPECT_TRUE(Artist::getByFilter(session, {}, {}, TrackArtistLinkType::Composer, Artist::SortMethod::ByName, std::nullopt, hasMore).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist.getId());

		artists = track->getArtists({TrackArtistLinkType::ReleaseArtist});
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist.getId());

		EXPECT_EQ(track->getArtistLinks().size(), 3);

		EXPECT_EQ(artist->getTracks().size(), 1);
		EXPECT_EQ(artist->getTracks({TrackArtistLinkType::ReleaseArtist}).size(), 1);
		EXPECT_EQ(artist->getTracks({TrackArtistLinkType::Artist}).size(), 1);
		EXPECT_EQ(artist->getTracks({TrackArtistLinkType::Writer}).size(), 1);
	}
}

TEST_F(DatabaseFixture,SingleTrackMultiArtists)
{
	ScopedTrack track {session, "track"};
	ScopedArtist artist1 {session, "artist1"};
	ScopedArtist artist2 {session, "artist2"};
	ASSERT_NE(artist1.getId(), artist2.getId());

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist1.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist2.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Artist::getAllOrphans(session).empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		ASSERT_EQ(artists.size(), 2);
		EXPECT_TRUE((artists[0]->getId() == artist1.getId() && artists[1]->getId() == artist2.getId())
			|| (artists[0]->getId() == artist2.getId() && artists[1]->getId() == artist1.getId()));

		EXPECT_EQ(track->getArtists({}).size(), 2);
		EXPECT_EQ(track->getArtists({TrackArtistLinkType::Artist}).size(), 2);
		EXPECT_TRUE(track->getArtists({TrackArtistLinkType::ReleaseArtist}).empty());
		EXPECT_EQ(Artist::getAll(session, Artist::SortMethod::ByName).size(), 2);
		EXPECT_EQ(Artist::getAllIds(session).size(), 2);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		EXPECT_EQ(artist1->getTracks().front(), track.get());
		EXPECT_EQ(artist2->getTracks().front(), track.get());

		EXPECT_TRUE(artist1->getTracks(TrackArtistLinkType::ReleaseArtist).empty());
		EXPECT_EQ(artist1->getTracks(TrackArtistLinkType::Artist).size(), 1);
		EXPECT_TRUE(artist2->getTracks(TrackArtistLinkType::ReleaseArtist).empty());
		EXPECT_EQ(artist2->getTracks(TrackArtistLinkType::Artist).size(), 1);
	}
}

TEST_F(DatabaseFixture, SingleArtistSearchByName)
{
	ScopedArtist artist {session, "AAA"};
	ScopedTrack track {session, "MyTrack"}; // filters does not work on orphans

	{
		auto transaction {session.createUniqueTransaction()};
		artist.get().modify()->setSortName("ZZZ");
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool more {};
		EXPECT_TRUE(Artist::getByFilter(session, {}, {"N"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more).empty());

		const auto artistsByAAA {Artist::Artist::getByFilter(session, {}, {"A"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more)};
		ASSERT_EQ(artistsByAAA.size(), 1);
		EXPECT_EQ(artistsByAAA.front()->getId(), artist.getId());

		const auto artistsByZZZ {Artist::Artist::getByFilter(session, {}, {"Z"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more)};
		ASSERT_EQ(artistsByZZZ.size(), 1);
		EXPECT_EQ(artistsByZZZ.front()->getId(), artist.getId());

		EXPECT_TRUE(Artist::getByName(session, "NNN").empty());
	}
}

TEST_F(DatabaseFixture, MultipleArtistsSearchByNameEscaped)
{
	ScopedArtist artist1 {session, "MyArtist%"};
	ScopedArtist artist2 {session, "%MyArtist"};
	ScopedArtist artist3 {session, "%_MyArtist"};

	ScopedArtist artist4 {session, "MyArtist%foo"};
	ScopedArtist artist5 {session, "foo%MyArtist"};
	ScopedArtist artist6 {session, "%AMyArtist"};

	{
		auto transaction {session.createSharedTransaction()};
		{
			const auto artists {Artist::getByName(session, "MyArtist%")};
			ASSERT_TRUE(artists.size() == 1);
			EXPECT_EQ(artists.front()->getId(), artist1.getId());
			EXPECT_TRUE(Artist::getByName(session, "MyArtistFoo").empty());
		}
		{
			const auto artists {Artist::getByName(session, "%MyArtist")};
			ASSERT_TRUE(artists.size() == 1);
			EXPECT_EQ(artists.front()->getId(), artist2.getId());
			EXPECT_TRUE(Artist::getByName(session, "FooMyArtist").empty());
		}
		{
			const auto artists {Artist::getByName(session, "%_MyArtist")};
			ASSERT_TRUE(artists.size() == 1);
			ASSERT_EQ(artists.front()->getId(), artist3.getId());
			EXPECT_TRUE(Artist::getByName(session, "%CMyArtist").empty());
		}
	}

	// get by filter only works with tracks links...
	ScopedTrack track {session, "MyTrack"}; // filters does not work on orphans

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track.get(), artist1.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist2.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist3.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist4.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist5.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track.get(), artist6.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		bool more;
		{
			const auto artists {Artist::getByFilter(session, {}, {"MyArtist"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more)};
			EXPECT_EQ(artists.size(), 6);
		}

		{
			const auto artists {Artist::getByFilter(session, {}, {"MyArtist%"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more)};
			ASSERT_EQ(artists.size(), 2);
			EXPECT_EQ(artists[0]->getId(), artist1.getId());
			EXPECT_EQ(artists[1]->getId(), artist4.getId());
		}

		{
			const auto artists {Artist::getByFilter(session, {}, {"%MyArtist"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more)};
			ASSERT_EQ(artists.size(), 2);
			EXPECT_EQ(artists[0]->getId(), artist2.getId());
			EXPECT_EQ(artists[1]->getId(), artist5.getId());
		}

		{
			const auto artists {Artist::getByFilter(session, {}, {"_MyArtist"}, std::nullopt, Artist::SortMethod::ByName, std::nullopt, more)};
			ASSERT_EQ(artists.size(), 1);
			EXPECT_EQ(artists[0]->getId(), artist3.getId());
		}
	}
}

TEST_F(DatabaseFixture, MultiArtistsSortMethod)
{
	ScopedArtist artistA {session, "artistA"};
	ScopedArtist artistB {session, "artistB"};

	{
		auto transaction {session.createUniqueTransaction()};

		artistA.get().modify()->setSortName("sortNameB");
		artistB.get().modify()->setSortName("sortNameA");
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto allArtistsByName {Artist::getAll(session, Artist::SortMethod::ByName)};
		auto allArtistsBySortName {Artist::getAll(session, Artist::SortMethod::BySortName)};

		ASSERT_EQ(allArtistsByName.size(), 2);
		EXPECT_EQ(allArtistsByName.front()->getId(), artistA.getId());
		EXPECT_EQ(allArtistsByName.back()->getId(), artistB.getId());

		ASSERT_EQ(allArtistsBySortName.size(), 2);
		EXPECT_EQ(allArtistsBySortName.front()->getId(), artistB.getId());
		EXPECT_EQ(allArtistsBySortName.back()->getId(), artistA.getId());
	}
}

TEST_F(DatabaseFixture, SingleArtistNonReleaseTracks)
{
	ScopedArtist artist {session, "artist"};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack2"};
	ScopedRelease release{session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_FALSE(artist->hasNonReleaseTracks(std::nullopt));

		bool moreResults;
		const auto tracks {artist->getNonReleaseTracks(std::nullopt, std::nullopt, moreResults )};
		EXPECT_EQ(tracks.size(), 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track1.get(), artist.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track2.get(), artist.get(), TrackArtistLinkType::Artist);

		track1.get().modify()->setRelease(release.get());
	}


	{
		auto transaction {session.createSharedTransaction()};

		bool moreResults;
		const auto tracks {artist->getNonReleaseTracks(std::nullopt, std::nullopt, moreResults )};
		EXPECT_TRUE(artist->hasNonReleaseTracks(std::nullopt));
		EXPECT_FALSE(moreResults);
		ASSERT_EQ(tracks.size(), 1);
		EXPECT_EQ(tracks.front()->getId(), track2.getId());
	}
}
