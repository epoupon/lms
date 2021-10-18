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

TEST_F(DatabaseFixture, SingleRelease)
{
	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(Release::exists(session, 0));
		EXPECT_FALSE(Release::exists(session, 1));
	}

	ScopedRelease release {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_TRUE(Release::exists(session, release.getId()));

		auto releases {Release::getAllOrphans(session)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front()->getId(), release.getId());

		releases = Release::getAll(session);
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front()->getId(), release.getId());
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
			EXPECT_EQ(release->getTracks().front()->getId(), track.getId());
		}

		{
			auto transaction {session.createUniqueTransaction()};

			ASSERT_TRUE(track->getRelease());
			EXPECT_EQ(track->getRelease()->getId(), release.getId());
		}

		{
			auto transaction {session.createUniqueTransaction()};
			auto tracks {Track::getByNameAndReleaseName(session, "MyTrackName", "MyReleaseName")};
			ASSERT_EQ(tracks.size(), 1);
			EXPECT_EQ(tracks.front()->getId(), track.getId());
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
		EXPECT_EQ(releases.front()->getId(), release.getId());
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
			EXPECT_TRUE(std::none_of(std::cbegin(releases), std::cend(releases), [&](const Release::pointer& release) { return release->getId() == release6.getId(); }));
		}
		{
			const auto releases {Release::getByFilter(session, {}, {"MyRelease%"}, std::nullopt, more)};
			ASSERT_EQ(releases.size(), 2);
			EXPECT_EQ(releases[0]->getId(), release2.getId());
			EXPECT_EQ(releases[1]->getId(), release4.getId());
		}
		{
			const auto releases {Release::getByFilter(session, {}, {"%MyRelease"}, std::nullopt, more)};
			ASSERT_EQ(releases.size(), 2);
			EXPECT_EQ(releases[0]->getId(), release3.getId());
			EXPECT_EQ(releases[1]->getId(), release5.getId());
		}
		{
			const auto releases {Release::getByFilter(session, {}, {"Foo%MyRelease"}, std::nullopt, more)};
			ASSERT_EQ(releases.size(), 1);
			EXPECT_EQ(releases[0]->getId(), release5.getId());
		}
		{
			const auto releases {Release::getByFilter(session, {}, {"MyRelease%Foo"}, std::nullopt, more)};
			ASSERT_EQ(releases.size(), 1);
			EXPECT_EQ(releases[0]->getId(), release4.getId());
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

		EXPECT_EQ(release1->getFirstTrack()->getId(), track1A.getId());
		EXPECT_EQ(release2->getFirstTrack()->getId(), track2B.getId());
	}
}

TEST_F(DatabaseFixture, MultiTracksSingleReleaseDate)
{
	ScopedRelease release1 {session, "MyRelease1"};
	ScopedRelease release2 {session, "MyRelease2"};
	const Wt::WDate release1Date {Wt::WDate {1994, 2, 3}};
	const Wt::WDate release1OriginalDate {Wt::WDate {1993, 4, 5}};

	ScopedTrack track1A {session, "MyTrack1A"};
	ScopedTrack track1B {session, "MyTrack1B"};
	ScopedTrack track2A {session, "MyTrack2A"};
	ScopedTrack track2B {session, "MyTrack2B"};

	{
		auto transaction {session.createSharedTransaction()};

		const auto releases {Release::getByYear(session, 0, 3000)};
		EXPECT_EQ(releases.size(), 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track1A.get().modify()->setRelease(release1.get());
		track1B.get().modify()->setRelease(release1.get());
		track2A.get().modify()->setRelease(release2.get());
		track2B.get().modify()->setRelease(release2.get());


		track1A.get().modify()->setDate(release1Date);
		track1B.get().modify()->setDate(release1Date);
		track1A.get().modify()->setOriginalDate(release1OriginalDate);
		track1B.get().modify()->setOriginalDate(release1OriginalDate);

		EXPECT_EQ(release1.get()->getReleaseYear(), release1Date.year());
		EXPECT_EQ(release1.get()->getReleaseYear(true), release1OriginalDate.year());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {Release::getByYear(session, 1950, 2000)};
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front()->getId(), release1.getId());

		releases = Release::getByYear(session, 1994, 1994);
		ASSERT_EQ(releases.size(), 1);
		EXPECT_EQ(releases.front()->getId(), release1.getId());

		releases = Release::getByYear(session, 1993, 1993);
		ASSERT_EQ(releases.size(), 0);
	}
}

