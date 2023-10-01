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

TEST_F(DatabaseFixture, Release)
{
	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(Release::getCount(session), 0);
		EXPECT_FALSE(Release::exists(session, 0));
		EXPECT_FALSE(Release::exists(session, 1));
	}

	ScopedRelease release {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_EQ(Release::getCount(session), 1);
		EXPECT_TRUE(Release::exists(session, release.getId()));

		{
			auto releases {Release::findOrphans(session, Range {})};
			ASSERT_EQ(releases.results.size(), 1);
			EXPECT_EQ(releases.results.front(), release.getId());
		}

		{
			auto releases {Release::find(session, Release::FindParameters {})};
			ASSERT_EQ(releases.results.size(), 1);
			EXPECT_EQ(releases.results.front(), release.getId());
			EXPECT_EQ(release->getDuration(), std::chrono::seconds {0});
		}
	}
}

TEST_F(DatabaseFixture, Release_singleTrack)
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
			EXPECT_TRUE(Release::findOrphans(session, Range {}).results.empty());

			const auto tracks {Track::find(session, Track::FindParameters {}.setRelease(release.getId()))};
			ASSERT_EQ(tracks.results.size(), 1);
			EXPECT_EQ(tracks.results.front(), track.getId());
		}

		{
			auto transaction {session.createUniqueTransaction()};

			ASSERT_TRUE(track->getRelease());
			EXPECT_EQ(track->getRelease()->getId(), release.getId());
		}

		{
			auto transaction {session.createUniqueTransaction()};
			auto tracks {Track::find(session, Track::FindParameters{}.setName("MyTrackName").setReleaseName("MyReleaseName"))};
			ASSERT_EQ(tracks.results.size(), 1);
			EXPECT_EQ(tracks.results.front(), track.getId());
		}
		{
			auto transaction {session.createUniqueTransaction()};
			auto tracks {Track::find(session, Track::FindParameters{}.setName("MyTrackName").setReleaseName("MyReleaseFoo"))};
			EXPECT_EQ(tracks.results.size(), 0);
		}
		{
			auto transaction {session.createUniqueTransaction()};
			auto tracks {Track::find(session, Track::FindParameters{}.setName("MyTrackFoo").setReleaseName("MyReleaseName"))};
			EXPECT_EQ(tracks.results.size(), 0);
		}
	}

	{
		auto transaction {session.createUniqueTransaction()};

		const auto tracks {Track::find(session, Track::FindParameters {}.setRelease(release.getId()))};
		EXPECT_TRUE(tracks.results.empty());

		auto releases {Release::findOrphans(session, Range {})};
		ASSERT_EQ(releases.results.size(), 1);
		EXPECT_EQ(releases.results.front(), release.getId());
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

		{
			const auto releases {Release::find(session, Release::FindParameters {}.setKeywords({"Release"}))};
			EXPECT_EQ(releases.results.size(), 6);
		}

		{
			const auto releases {Release::find(session, Release::FindParameters {}.setKeywords({"MyRelease"}))};
			ASSERT_EQ(releases.results.size(), 5);
			EXPECT_TRUE(std::none_of(std::cbegin(releases.results), std::cend(releases.results), [&](const ReleaseId releaseId) { return releaseId == release6.getId(); }));
		}
		{
			const auto releases {Release::find(session, Release::FindParameters {}.setKeywords({"MyRelease%"}))};
			ASSERT_EQ(releases.results.size(), 2);
			EXPECT_EQ(releases.results[0], release2.getId());
			EXPECT_EQ(releases.results[1], release4.getId());
		}
		{
			const auto releases {Release::find(session, Release::FindParameters {}.setKeywords({"%MyRelease"}))};
			ASSERT_EQ(releases.results.size(), 2);
			EXPECT_EQ(releases.results[0], release3.getId());
			EXPECT_EQ(releases.results[1], release5.getId());
		}
		{
			const auto releases {Release::find(session, Release::FindParameters {}.setKeywords({"Foo%MyRelease"}))};
			ASSERT_EQ(releases.results.size(), 1);
			EXPECT_EQ(releases.results[0], release5.getId());
		}
		{
			const auto releases {Release::find(session, Release::FindParameters {}.setKeywords({"MyRelease%Foo"}))};
			ASSERT_EQ(releases.results.size(), 1);
			EXPECT_EQ(releases.results[0], release4.getId());
		}
	}
}

TEST_F(DatabaseFixture, MultiTracksSingleReleaseTotalDiscTrack)
{
	ScopedRelease release1 {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(release1->getTotalDisc());
	}

	ScopedTrack track1 {session, "MyTrack"};
	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setRelease(release1.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(release1->getTotalDisc());
	}

	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setTotalTrack(36);
		release1.get().modify()->setTotalDisc(6);
	}

	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_TRUE(track1->getTotalTrack());
		EXPECT_EQ(*track1->getTotalTrack(), 36);
		ASSERT_TRUE(release1->getTotalDisc());
		EXPECT_EQ(*release1->getTotalDisc(), 6);
	}

	ScopedTrack track2 {session, "MyTrack2"};
	{
		auto transaction {session.createUniqueTransaction()};

		track2.get().modify()->setRelease(release1.get());
		track2.get().modify()->setTotalTrack(37);
		release1.get().modify()->setTotalDisc(67);
	}

	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_TRUE(track1->getTotalTrack());
		EXPECT_EQ(*track1->getTotalTrack(), 36);
		ASSERT_TRUE(release1->getTotalDisc());
		EXPECT_EQ(*release1->getTotalDisc(), 67);
	}

	ScopedRelease release2 {session, "MyRelease2"};
	{
		auto transaction {session.createSharedTransaction()};

		EXPECT_FALSE(release2->getTotalDisc());
	}

	ScopedTrack track3 {session, "MyTrack3"};
	{
		auto transaction {session.createUniqueTransaction()};

		track3.get().modify()->setRelease(release2.get());
		track3.get().modify()->setTotalTrack(7);
		release2.get().modify()->setTotalDisc(5);
	}
	{
		auto transaction {session.createSharedTransaction()};

		ASSERT_TRUE(track1->getTotalTrack());
		EXPECT_EQ(*track1->getTotalTrack(), 36);
		ASSERT_TRUE(release1->getTotalDisc());
		EXPECT_EQ(*release2->getTotalDisc(), 5);
		ASSERT_TRUE(track3->getTotalTrack());
		EXPECT_EQ(*track3->getTotalTrack(), 7);
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

		EXPECT_TRUE(Track::find(session, Track::FindParameters {}.setRelease(release1.getId())).results.empty());
		EXPECT_TRUE(Track::find(session, Track::FindParameters {}.setRelease(release2.getId())).results.empty());
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

		{
			const auto tracks {Track::find(session, Track::FindParameters {}.setRelease(release1.getId()).setSortMethod(TrackSortMethod::Release))};
			ASSERT_FALSE(tracks.results.empty());
			EXPECT_EQ(tracks.results.front(), track1A.getId());
		}

		{
			const auto tracks {Track::find(session, Track::FindParameters {}.setRelease(release2.getId()).setSortMethod(TrackSortMethod::Release))};
			ASSERT_FALSE(tracks.results.empty());
			EXPECT_EQ(tracks.results.front(), track2B.getId());
		}
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

		const auto releases {Release::find(session, Release::FindParameters {}.setDateRange(DateRange::fromYearRange(0, 3000)))};
		EXPECT_EQ(releases.results.size(), 0);
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

		EXPECT_EQ(release1.get()->getReleaseDate(), release1Date);
		EXPECT_EQ(release1.get()->getOriginalReleaseDate(), release1OriginalDate);
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {Release::find(session, Release::FindParameters {}.setDateRange(DateRange::fromYearRange(1950, 2000)))};
		ASSERT_EQ(releases.results.size(), 1);
		EXPECT_EQ(releases.results.front(), release1.getId());

		releases = Release::find(session, Release::FindParameters {}.setDateRange(DateRange::fromYearRange(1994, 1994)));
		ASSERT_EQ(releases.results.size(), 1);
		EXPECT_EQ(releases.results.front(), release1.getId());

		releases = Release::find(session, Release::FindParameters {}.setDateRange(DateRange::fromYearRange(1993, 1993)));
		ASSERT_EQ(releases.results.size(), 0);
	}
}

TEST_F(DatabaseFixture, Release_writtenAfter)
{
	ScopedRelease release {session, "MyRelease"};
	ScopedTrack track {session, "MyTrack"};

	const Wt::WDateTime dateTime {Wt::WDate {1950, 1, 1}, Wt::WTime {12, 30, 20}};

	{
		auto transaction {session.createUniqueTransaction()};
		track.get().modify()->setLastWriteTime(dateTime);
		track.get().modify()->setRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};
		const auto releases {Release::find(session, Release::FindParameters {})};
		EXPECT_EQ(releases.results.size(), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};
		const auto releases {Release::find(session, Release::FindParameters {}.setWrittenAfter(dateTime.addSecs(-1)))};
		EXPECT_EQ(releases.results.size(), 1);
	}

	{
		auto transaction {session.createSharedTransaction()};
		const auto releases {Release::find(session, Release::FindParameters {}.setWrittenAfter(dateTime.addSecs(+1)))};
		EXPECT_EQ(releases.results.size(), 0);
	}
}

TEST_F(DatabaseFixture, Release_artist)
{
	ScopedRelease release {session, "MyRelease"};
	ScopedTrack track {session, "MyTrack"};
	ScopedArtist artist {session, "MyArtist"};
	ScopedArtist artist2 {session, "MyArtist2"};
	{
		auto transaction {session.createUniqueTransaction()};
		track.get().modify()->setRelease(release.get());
	}

	{
		auto transaction {session.createSharedTransaction()};

		auto releases {Release::find(session, Release::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::Artist}))};
		EXPECT_EQ(releases.results.size(), 0);

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist2.getId(), {TrackArtistLinkType::Artist}));
		EXPECT_EQ(releases.results.size(), 0);
	}

	{
		auto transaction {session.createUniqueTransaction()};
		TrackArtistLink::create(session, track.get(), artist.get(), TrackArtistLinkType::Artist);

		auto releases {Release::find(session, Release::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::Artist}))};
		ASSERT_EQ(releases.results.size(), 1);
		EXPECT_EQ(releases.results.front(), release.getId());

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::Artist, TrackArtistLinkType::Mixer}));
		EXPECT_EQ(releases.results.size(), 1);
		EXPECT_EQ(releases.results.front(), release.getId());

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist2.getId(), {TrackArtistLinkType::Artist}));
		EXPECT_EQ(releases.results.size(), 0);

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist2.getId()));
		EXPECT_EQ(releases.results.size(), 0);

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::ReleaseArtist, TrackArtistLinkType::Artist}));
		ASSERT_EQ(releases.results.size(), 1);
		EXPECT_EQ(releases.results.front(), release.getId());

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist.getId()));
		ASSERT_EQ(releases.results.size(), 1);
		EXPECT_EQ(releases.results.front(), release.getId());

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::Composer}));
		EXPECT_EQ(releases.results.size(), 0);

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist.getId(), {TrackArtistLinkType::Composer, TrackArtistLinkType::Mixer}));
		EXPECT_EQ(releases.results.size(), 0);

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist.getId(), {}, {TrackArtistLinkType::Artist}));
		EXPECT_EQ(releases.results.size(), 0);

		releases = Release::find(session, Release::FindParameters {}.setArtist(artist.getId(), {}, {TrackArtistLinkType::Artist, TrackArtistLinkType::Composer}));
		EXPECT_EQ(releases.results.size(), 0);
	}
}

TEST_F(DatabaseFixture, Release_getDiscCount)
{
	ScopedRelease release {session, "MyRelease"};
	ScopedTrack track {session, "MyTrack"};
	ScopedTrack track2 {session, "MyTrack2"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(release.get()->getDiscCount(), 0);
	}
	{
		auto transaction {session.createUniqueTransaction()};
		track.get().modify()->setRelease(release.get());
	}
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(release.get()->getDiscCount(), 0);
	}
	{
		auto transaction {session.createUniqueTransaction()};
		track.get().modify()->setDiscNumber(5);
	}
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(release.get()->getDiscCount(), 1);
	}
	{
		auto transaction {session.createUniqueTransaction()};
		track2.get().modify()->setRelease(release.get());
		track2.get().modify()->setDiscNumber(5);
	}
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(release.get()->getDiscCount(), 1);
	}
	{
		auto transaction {session.createUniqueTransaction()};
		track2.get().modify()->setDiscNumber(6);
	}
	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(release.get()->getDiscCount(), 2);
	}
}

TEST_F(DatabaseFixture, Release_releaseType)
{
	ScopedRelease release {session, "MyRelease"};

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(release.get()->getPrimaryType(), std::nullopt);
		EXPECT_EQ(release.get()->getSecondaryTypes(), EnumSet<ReleaseTypeSecondary> {});
	}

	{
		auto transaction {session.createUniqueTransaction()};
		release.get().modify()->setPrimaryType({ ReleaseTypePrimary::Album });
		release.get().modify()->setSecondaryTypes({ ReleaseTypeSecondary::Compilation });
	}

	{
		auto transaction {session.createSharedTransaction()};
		EXPECT_EQ(release.get()->getPrimaryType(), ReleaseTypePrimary::Album);
		EXPECT_TRUE(release.get()->getSecondaryTypes().contains(ReleaseTypeSecondary::Compilation));
	}
}

TEST_F(DatabaseFixture, ReleaseSortOrder)
{
	ScopedRelease release1 {session, "MyRelease1"};
	const Wt::WDate release1Date {Wt::WDate {2000, 2, 3}};
	const Wt::WDate release1OriginalDate {Wt::WDate {1993, 4, 5}};

	ScopedRelease release2 {session, "MyRelease2"};
	const Wt::WDate release2Date {Wt::WDate {1994, 2, 3}};

	ScopedTrack track1 {session, "MyTrack1"};
	ScopedTrack track2 {session, "MyTrack2"};

	ASSERT_LT(release2Date, release1Date);
	ASSERT_GT(release2Date, release1OriginalDate);

	{
		auto transaction {session.createUniqueTransaction()};

		track1.get().modify()->setRelease(release1.get());
		track1.get().modify()->setOriginalDate(release1OriginalDate);
		track1.get().modify()->setDate(release1Date);

		track2.get().modify()->setRelease(release2.get());
		track2.get().modify()->setDate(release2Date);
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto releases {Release::find(session, Release::FindParameters {}.setSortMethod(ReleaseSortMethod::Name) )};
		ASSERT_EQ(releases.results.size(), 2);
		EXPECT_EQ(releases.results.front(), release1.getId());
		EXPECT_EQ(releases.results.back(), release2.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto releases {Release::find(session, Release::FindParameters {}.setSortMethod(ReleaseSortMethod::Random) )};
		ASSERT_EQ(releases.results.size(), 2);
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto releases {Release::find(session, Release::FindParameters {}.setSortMethod(ReleaseSortMethod::Date) )};
		ASSERT_EQ(releases.results.size(), 2);
		EXPECT_EQ(releases.results.front(), release2.getId());
		EXPECT_EQ(releases.results.back(), release1.getId());
	}

	{
		auto transaction {session.createSharedTransaction()};

		const auto releases {Release::find(session, Release::FindParameters {}.setSortMethod(ReleaseSortMethod::OriginalDate) )};
		ASSERT_EQ(releases.results.size(), 2);
		EXPECT_EQ(releases.results.front(), release1.getId());
		EXPECT_EQ(releases.results.back(), release2.getId());
	}
	{
		auto transaction {session.createSharedTransaction()};

		const auto releases {Release::find(session, Release::FindParameters {}.setSortMethod(ReleaseSortMethod::OriginalDateDesc) )};
		ASSERT_EQ(releases.results.size(), 2);
		EXPECT_EQ(releases.results.front(), release2.getId());
		EXPECT_EQ(releases.results.back(), release1.getId());
	}
}

