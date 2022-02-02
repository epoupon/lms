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

TEST_F(DatabaseFixture, Track)
{
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(Track::find(session, Track::FindParameters {}).results.size(), 0);
		EXPECT_EQ(Track::getCount(session), 0);
		EXPECT_FALSE(Track::exists(session, 0));
	}

	ScopedTrack track {session, "MyTrackFile"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(Track::find(session, Track::FindParameters {}).results.size(), 1);
		EXPECT_EQ(Track::getCount(session), 1);
		EXPECT_TRUE(Track::exists(session, track.getId()));
		auto myTrack {Track::find(session, track.getId())};
		ASSERT_TRUE(myTrack);
		EXPECT_EQ(myTrack->getId(), track.getId());
	}
}

TEST_F(DatabaseFixture, MultipleTracks)
{
	ScopedTrack track1 {session, "MyTrackFile1"};
	ScopedTrack track2 {session, "MyTrackFile2"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(track1.getId() != track2.getId());
		EXPECT_TRUE(track1.get() != track2.get());
		EXPECT_FALSE(track1.get() == track2.get());
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

		{
			const auto tracks {Track::find(session, Track::FindParameters {}.setKeywords({"Track"}))};
			EXPECT_EQ(tracks.results.size(), 6);
		}
		{
			const auto tracks {Track::find(session, Track::FindParameters {}.setKeywords({"MyTrack"}))};
			EXPECT_EQ(tracks.results.size(), 5);
			EXPECT_TRUE(std::none_of(std::cbegin(tracks.results), std::cend(tracks.results), [&](const TrackId trackId) { return trackId == track6.getId(); }));
		}
		{
			const auto tracks {Track::find(session, Track::FindParameters {}.setKeywords({"MyTrack%"}))};
			ASSERT_EQ(tracks.results.size(), 2);
			EXPECT_EQ(tracks.results[0], track2.getId());
			EXPECT_EQ(tracks.results[1], track3.getId());
		}
		{
			const auto tracks {Track::find(session, Track::FindParameters {}.setKeywords({"%MyTrack"}))};
			ASSERT_EQ(tracks.results.size(), 2);
			EXPECT_EQ(tracks.results[0], track4.getId());
			EXPECT_EQ(tracks.results[1], track5.getId());
		}
	}
}

TEST_F(DatabaseFixture, Track_date)
{
	ScopedTrack track {session, "MyTrack"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(track->getYear(), std::nullopt);
		EXPECT_EQ(track->getOriginalYear(), std::nullopt);
	}

	{
		auto transaction {session.createUniqueTransaction()};
		track.get().modify()->setDate(Wt::WDate {1995, 5, 5});
		track.get().modify()->setOriginalDate(Wt::WDate {1994, 2, 2});
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(track->getYear(), 1995);
		EXPECT_EQ(track->getOriginalYear(), 1994);
	}
}

TEST_F(DatabaseFixture, Track_writtenAfter)
{
	ScopedTrack track {session, "MyTrack"};

	const Wt::WDateTime dateTime {Wt::WDate {1950, 1, 1}, Wt::WTime {12, 30, 20}};

	{
		auto transaction {session.createUniqueTransaction()};
		track.get().modify()->setLastWriteTime(dateTime);
	}

	{
		auto transaction {session.createSharedTransaction()};
		const auto tracks {Track::find(session, Track::FindParameters {})};
		EXPECT_EQ(tracks.results.size(), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};
		const auto tracks {Track::find(session, Track::FindParameters {}.setWrittenAfter(dateTime.addSecs(-1)))};
		EXPECT_EQ(tracks.results.size(), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};
		const auto tracks {Track::find(session, Track::FindParameters {}.setWrittenAfter(dateTime.addSecs(+1)))};
		EXPECT_EQ(tracks.results.size(), 0);
	}
}

