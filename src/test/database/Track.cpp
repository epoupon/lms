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

using namespace Database;

TEST_F(DatabaseFixture, SingleTrack)
{
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(Track::getCount(session), 0);
	}

	ScopedTrack track {session, "MyTrackFile"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(Track::getAll(session).size(), 1);
		EXPECT_EQ(Track::getCount(session), 1);
	}
}

TEST_F(DatabaseFixture, MultipleTracksSearchByFilter)
{
	ScopedTrack track1 {session, ""};
	ScopedTrack track2 {session, ""};
	ScopedTrack track3 {session, ""};
	ScopedTrack track4 {session, ""};
	ScopedTrack track5 {session, ""};
	ScopedTrack track6 {session, ""};

	{
		auto transaction {session.createUniqueTransaction()};
		track1.get().modify()->setName("MyTrack");
		track2.get().modify()->setName("MyTrack%");
		track3.get().modify()->setName("MyTrack%Foo");
		track4.get().modify()->setName("%MyTrack");
		track5.get().modify()->setName("Foo%MyTrack");
		track6.get().modify()->setName("M_Track");
	}

	{
		auto transaction {session.createSharedTransaction()};

		bool more;
		{
			const auto tracks {Track::getByFilter(session, {}, {"Track"}, std::nullopt, more)};
			EXPECT_EQ(tracks.size(), 6);
		}
		{
			const auto tracks {Track::getByFilter(session, {}, {"MyTrack"}, std::nullopt, more)};
			EXPECT_EQ(tracks.size(), 5);
			EXPECT_TRUE(std::none_of(std::cbegin(tracks), std::cend(tracks), [&](const Track::pointer& track) { return track.id() == track6.getId(); }));
		}
		{
			const auto tracks {Track::getByFilter(session, {}, {"MyTrack%"}, std::nullopt, more)};
			ASSERT_EQ(tracks.size(), 2);
			EXPECT_EQ(tracks[0].id(), track2.getId());
			EXPECT_EQ(tracks[1].id(), track3.getId());
		}
		{
			const auto tracks {Track::getByFilter(session, {}, {"%MyTrack"}, std::nullopt, more)};
			ASSERT_EQ(tracks.size(), 2);
			EXPECT_EQ(tracks[0].id(), track4.getId());
			EXPECT_EQ(tracks[1].id(), track5.getId());
		}
	}
}

