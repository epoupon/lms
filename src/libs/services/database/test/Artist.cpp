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

TEST_F(DatabaseFixture, Artist)
{
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_FALSE(Artist::exists(session, 35));
		EXPECT_FALSE(Artist::exists(session, 0));
		EXPECT_FALSE(Artist::exists(session, 1));
		EXPECT_EQ(Artist::getCount(session), 0);
	}

	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(artist.get());
		EXPECT_FALSE(!artist.get());
		EXPECT_EQ(artist.get()->getId(), artist.getId());

		EXPECT_TRUE(Artist::exists(session, artist.getId()));
		EXPECT_EQ(Artist::getCount(session), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::find(session, Artist::FindParameters {})};
		ASSERT_EQ(artists.results.size(), 1);
		EXPECT_EQ(artists.results.front(), artist.getId());

		artists = Artist::findAllOrphans(session, Range {});
		ASSERT_EQ(artists.results.size(), 1);
		EXPECT_EQ(artists.results.front(), artist.getId());
	}
}

TEST_F(DatabaseFixture, Artist_singleTrack)
{
	ScopedTrack track {session, "MyTrack"};
	ScopedArtist artist {session, "MyArtist"};

	{
		auto transaction {session.createUniqueTransaction()};

		track.get().modify()->setName("MyTrackName");
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_TRUE(Artist::findAllOrphans(session, Range {}).results.empty());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front()->getId(), artist.getId());

		ASSERT_EQ(track->getArtistLinks().size(), 1);
		auto artistLink {track->getArtistLinks().front()};
		EXPECT_EQ(artistLink->getTrack()->getId(), track.getId());
		EXPECT_EQ(artistLink->getArtist()->getId(), artist.getId());

		ASSERT_EQ(track->getArtists({TrackArtistLinkType::Artist}).size(), 1);
		EXPECT_TRUE(track->getArtists({TrackArtistLinkType::ReleaseArtist}).empty());
		EXPECT_EQ(track->getArtists({}).size(), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {track->getArtistIds({TrackArtistLinkType::Artist})};
		ASSERT_EQ(artists.size(), 1);
		EXPECT_EQ(artists.front(), artist.getId());

		ASSERT_EQ(track->getArtistIds({TrackArtistLinkType::Artist}).size(), 1);
		EXPECT_TRUE(track->getArtistIds({TrackArtistLinkType::ReleaseArtist}).empty());
		EXPECT_EQ(track->getArtistIds({}).size(), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};
		auto tracks {Track::find(session, Track::FindParameters{}.setName("MyTrackName").setArtistName("MyArtist"))};
		ASSERT_EQ(tracks.results.size(), 1);
		EXPECT_EQ(tracks.results.front(), track.getId());
	}
	{
		auto transaction {session.createSharedTransaction()};
		auto tracks {Track::find(session, Track::FindParameters{}.setName("MyTrackName").setArtistName("MyArtistFoo"))};
		EXPECT_EQ(tracks.results.size(), 0);
	}
	{
		auto transaction {session.createSharedTransaction()};
		auto tracks {Track::find(session, Track::FindParameters{}.setName("MyTrackNameFoo").setArtistName("MyArtist"))};
		EXPECT_EQ(tracks.results.size(), 0);
	}
	{
		auto transaction {session.createSharedTransaction()};
		auto artists {Artist::find(session, Artist::FindParameters{}.setTrack(track->getId()))};
		ASSERT_EQ(artists.results.size(), 1);
		EXPECT_EQ(artists.results.front(), artist.getId());
	}
}

TEST_F(DatabaseFixture, Artist_singleTracktMultiRoles)
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
		EXPECT_TRUE(Artist::findAllOrphans(session, Range {}).results.empty());
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(Artist::find(session, Artist::FindParameters {}).results.size(), 1);
		EXPECT_EQ(Artist::find(session, Artist::FindParameters {}.setLinkType(TrackArtistLinkType::Artist)).results.size(), 1);
		EXPECT_EQ(Artist::find(session, Artist::FindParameters {}.setLinkType(TrackArtistLinkType::ReleaseArtist)).results.size(), 1);
		EXPECT_EQ(Artist::find(session, Artist::FindParameters {}.setLinkType(TrackArtistLinkType::Writer)).results.size(), 1);
		EXPECT_EQ(Artist::find(session, Artist::FindParameters {}.setLinkType(TrackArtistLinkType::Composer)).results.size(), 0);
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

		auto tracks {Track::find(session, Track::FindParameters {}.setArtist(artist.getId()))};
		EXPECT_EQ(tracks.results.size(), 1);

		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::ReleaseArtist}));
		EXPECT_EQ(tracks.results.size(), 1);
		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::Artist}));
		EXPECT_EQ(tracks.results.size(), 1);
		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::Writer}));
		EXPECT_EQ(tracks.results.size(), 1);

		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::Composer}));
		EXPECT_EQ(tracks.results.size(), 0);
	}

	{
		auto transaction {session.createSharedTransaction()};
		EnumSet<TrackArtistLinkType> types{ TrackArtistLink::findUsedTypes(session, artist.getId()) };
		EXPECT_TRUE(types.contains(TrackArtistLinkType::ReleaseArtist));
		EXPECT_TRUE(types.contains(TrackArtistLinkType::Artist));
		EXPECT_TRUE(types.contains(TrackArtistLinkType::Writer));
		EXPECT_FALSE(types.contains(TrackArtistLinkType::Composer));
	}
}

TEST_F(DatabaseFixture, Artist_singleTrackMultiArtists)
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
		EXPECT_TRUE(Artist::findAllOrphans(session, Range {}).results.empty());
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
		EXPECT_EQ(Artist::find(session, Artist::FindParameters {}).results.size(), 2);
		EXPECT_EQ(Artist::find(session, Artist::FindParameters {}.setSortMethod(ArtistSortMethod::Random)).results.size(), 2);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto tracks {Track::find(session, Track::FindParameters {}.setArtist(artist1->getId()))};
		ASSERT_EQ(tracks.results.size(), 1);
		EXPECT_EQ(tracks.results.front(), track->getId());

		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist2->getId()));
		ASSERT_EQ(tracks.results.size(), 1);
		EXPECT_EQ(tracks.results.front(), track->getId());

		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist1->getId(), {TrackArtistLinkType::ReleaseArtist}));
		EXPECT_EQ(tracks.results.size(), 0);

		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist1->getId(), {TrackArtistLinkType::Artist}));
		EXPECT_EQ(tracks.results.size(), 1);

		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist2->getId(), {TrackArtistLinkType::ReleaseArtist}));
		EXPECT_EQ(tracks.results.size(), 0);

		tracks = Track::find(session, Track::FindParameters {}.setArtist(artist2->getId(), {TrackArtistLinkType::Artist}));
		EXPECT_EQ(tracks.results.size(), 1);
	}
}

TEST_F(DatabaseFixture, Artist_findByName)
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

		EXPECT_TRUE(Artist::find(session, Artist::FindParameters {}.setKeywords({"N"})).results.empty());

		const auto artistsByAAA {Artist::find(session, Artist::FindParameters {}.setKeywords({"A"}))};
		ASSERT_EQ(artistsByAAA.results.size(), 1);
		EXPECT_EQ(artistsByAAA.results.front(), artist.getId());

		const auto artistsByZZZ {Artist::Artist::find(session, Artist::FindParameters {}.setKeywords({"Z"}))};
		ASSERT_EQ(artistsByZZZ.results.size(), 1);
		EXPECT_EQ(artistsByZZZ.results.front(), artist.getId());

		EXPECT_TRUE(Artist::find(session, "NNN").empty());
		EXPECT_EQ(Artist::find(session, "AAA").size(), 1);
	}
}

TEST_F(DatabaseFixture, Artist_findByNameEscaped)
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
			const auto artists {Artist::find(session, "MyArtist%")};
			ASSERT_TRUE(artists.size() == 1);
			EXPECT_EQ(artists.front()->getId(), artist1.getId());
			EXPECT_TRUE(Artist::find(session, "MyArtistFoo").empty());
		}
		{
			const auto artists {Artist::find(session, "%MyArtist")};
			ASSERT_TRUE(artists.size() == 1);
			EXPECT_EQ(artists.front()->getId(), artist2.getId());
			EXPECT_TRUE(Artist::find(session, "FooMyArtist").empty());
		}
		{
			const auto artists {Artist::find(session, "%_MyArtist")};
			ASSERT_TRUE(artists.size() == 1);
			ASSERT_EQ(artists.front()->getId(), artist3.getId());
			EXPECT_TRUE(Artist::find(session, "%CMyArtist").empty());
		}
	}

	{
		auto transaction {session.createSharedTransaction()};
		{
			const auto artists {Artist::find(session, Artist::FindParameters {}.setKeywords({"MyArtist"}))};
			EXPECT_EQ(artists.results.size(), 6);
		}

		{
			const auto artists {Artist::find(session, Artist::FindParameters {}.setKeywords({"MyArtist%"}).setSortMethod(ArtistSortMethod::ByName))};
			ASSERT_EQ(artists.results.size(), 2);
			EXPECT_EQ(artists.results[0], artist1.getId());
			EXPECT_EQ(artists.results[1], artist4.getId());
		}

		{
			const auto artists {Artist::find(session, Artist::FindParameters {}.setKeywords({"%MyArtist"}).setSortMethod(ArtistSortMethod::ByName))};
			ASSERT_EQ(artists.results.size(), 2);
			EXPECT_EQ(artists.results[0], artist2.getId());
			EXPECT_EQ(artists.results[1], artist5.getId());
		}

		{
			const auto artists {Artist::find(session, Artist::FindParameters {}.setKeywords({"_MyArtist"}).setSortMethod(ArtistSortMethod::ByName))};
			ASSERT_EQ(artists.results.size(), 1);
			EXPECT_EQ(artists.results[0], artist3.getId());
		}
	}
}

TEST_F(DatabaseFixture, Artist_sortMethod)
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

		auto allArtistsByName {Artist::find(session, Artist::FindParameters {}.setSortMethod(ArtistSortMethod::ByName))};
		auto allArtistsBySortName {Artist::find(session, Artist::FindParameters {}.setSortMethod(ArtistSortMethod::BySortName))};

		ASSERT_EQ(allArtistsByName.results.size(), 2);
		EXPECT_EQ(allArtistsByName.results.front(), artistA.getId());
		EXPECT_EQ(allArtistsByName.results.back(), artistB.getId());

		ASSERT_EQ(allArtistsBySortName.results.size(), 2);
		EXPECT_EQ(allArtistsBySortName.results.front(), artistB.getId());
		EXPECT_EQ(allArtistsBySortName.results.back(), artistA.getId());
	}
}

TEST_F(DatabaseFixture, Artist_nonReleaseTracks)
{
	ScopedArtist artist {session, "artist"};
	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack2"};
	ScopedRelease release{session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		auto tracks {Track::find(session, Track::FindParameters {}.setNonRelease(true).setArtist(artist->getId()))};
		EXPECT_EQ(tracks.results.size(), 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		TrackArtistLink::create(session, track1.get(), artist.get(), TrackArtistLinkType::Artist);
		TrackArtistLink::create(session, track2.get(), artist.get(), TrackArtistLinkType::Artist);

		track1.get().modify()->setRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto tracks {Track::find(session, Track::FindParameters {}.setArtist(artist.getId()).setNonRelease(true))};
		ASSERT_EQ(tracks.results.size(), 1);
		EXPECT_EQ(tracks.results.front(), track2.getId());
	}
}

TEST_F(DatabaseFixture, Artist_findByRelease)
{
	ScopedArtist artist {session, "artist"};
	ScopedTrack track {session, "MyTrack"};
	ScopedRelease release{session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};
		const auto artists {Artist::find(session, Artist::FindParameters {}.setRelease(release.getId()))};
		EXPECT_EQ(artists.results.size(), 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);
	}

	{
		auto transaction {session.createSharedTransaction()};
		const auto artists {Artist::find(session, Artist::FindParameters {}.setRelease(release.getId()))};
		EXPECT_EQ(artists.results.size(), 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};
		track.get().modify()->setRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		const auto artists {Artist::find(session, Artist::FindParameters {}.setRelease(release.getId()))};
		ASSERT_EQ(artists.results.size(), 1);
		EXPECT_EQ(artists.results.front(), artist.getId());
	}
}

