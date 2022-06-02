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
#include "services/database/StarredArtist.hpp"

using namespace Database;

using ScopedStarredArtist = ScopedEntity<Database::StarredArtist>;

TEST_F(DatabaseFixture, StarredArtist)
{
	ScopedArtist artist {session, "MyArtist"};
	ScopedUser user {session, "MyUser"};
	ScopedUser user2 {session, "MyUser2"};

	{
		auto transaction {session.createSharedTransaction()};

		auto starredArtist {StarredArtist::find(session, artist->getId(), user->getId(), Scrobbler::Internal)};
		EXPECT_FALSE(starredArtist);
		EXPECT_EQ(StarredArtist::getCount(session), 0);

		auto artists {Artist::find(session, Artist::FindParameters {})};
		EXPECT_EQ(artists.results.size(), 1);
	}

	ScopedStarredArtist starredArtist {session, artist.lockAndGet(), user.lockAndGet(), Scrobbler::Internal};
	{
		auto transaction {session.createSharedTransaction()};

		auto gotArtist {StarredArtist::find(session, artist->getId(), user->getId(), Scrobbler::Internal)};
		EXPECT_EQ(gotArtist->getId(), starredArtist->getId());
		EXPECT_EQ(StarredArtist::getCount(session), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::find(session, Artist::FindParameters {})};
		EXPECT_EQ(artists.results.size(), 1);

		artists = Artist::find(session, Artist::FindParameters {}.setStarringUser(user.getId(), Scrobbler::Internal));
		EXPECT_EQ(artists.results.size(), 1);

		artists = Artist::find(session, Artist::FindParameters {}.setStarringUser(user2.getId(), Scrobbler::Internal));
		EXPECT_EQ(artists.results.size(), 0);
	}
}

TEST_F(DatabaseFixture, StarredArtist_PendingDestroy)
{
	ScopedArtist artist {session, "MyArtist"};
	ScopedUser user {session, "MyUser"};
	ScopedStarredArtist starredArtist {session, artist.lockAndGet(), user.lockAndGet(), Scrobbler::Internal};

	{
		auto transaction {session.createUniqueTransaction()};

		auto artists {Artist::find(session, Artist::FindParameters {}.setStarringUser(user.getId(), Scrobbler::Internal))};
		EXPECT_EQ(artists.results.size(), 1);

		starredArtist.get().modify()->setScrobblingState(ScrobblingState::PendingRemove);
		artists = Artist::find(session, Artist::FindParameters {}.setStarringUser(user.getId(), Scrobbler::Internal));
		EXPECT_EQ(artists.results.size(), 0);
	}
}

TEST_F(DatabaseFixture, StarredArtist_dateTime)
{
	ScopedArtist artist1 {session, "MyArtist1"};
	ScopedArtist artist2 {session, "MyArtist2"};
	ScopedUser user {session, "MyUser"};

	ScopedStarredArtist starredArtist1 {session, artist1.lockAndGet(), user.lockAndGet(), Scrobbler::Internal};
	ScopedStarredArtist starredArtist2 {session, artist2.lockAndGet(), user.lockAndGet(), Scrobbler::Internal};

	const Wt::WDateTime dateTime {Wt::WDate {1950, 1, 2}, Wt::WTime {12, 30, 1}};

	{
		auto transaction {session.createSharedTransaction()};

		auto artists {Artist::find(session, Artist::FindParameters {}.setStarringUser(user.getId(), Scrobbler::Internal))};
		EXPECT_EQ(artists.results.size(), 2);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		starredArtist1.get().modify()->setDateTime(dateTime);
		starredArtist2.get().modify()->setDateTime(dateTime.addSecs(-1));

		auto artists {Artist::find(session, Artist::FindParameters {}.setStarringUser(user.getId(), Scrobbler::Internal)
																		.setSortMethod(ArtistSortMethod::StarredDateDesc))};
		ASSERT_EQ(artists.results.size(), 2);
		EXPECT_EQ(artists.results[0], starredArtist1->getArtist()->getId());
		EXPECT_EQ(artists.results[1], starredArtist2->getArtist()->getId());
	}
	{
		auto transaction {session.createUniqueTransaction()};

		starredArtist1.get().modify()->setDateTime(dateTime);
		starredArtist2.get().modify()->setDateTime(dateTime.addSecs(1));

		auto artists {Artist::find(session, Artist::FindParameters {}.setStarringUser(user.getId(), Scrobbler::Internal)
																		.setSortMethod(ArtistSortMethod::StarredDateDesc))};
		ASSERT_EQ(artists.results.size(), 2);
		EXPECT_EQ(artists.results[0], starredArtist2->getArtist()->getId());
		EXPECT_EQ(artists.results[1], starredArtist1->getArtist()->getId());
	}
}
