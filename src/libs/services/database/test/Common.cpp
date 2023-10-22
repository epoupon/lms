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

#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Db.hpp"
#include "services/database/Listen.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/StarredArtist.hpp"
#include "services/database/StarredRelease.hpp"
#include "services/database/StarredTrack.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "services/database/TrackBookmark.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/Types.hpp"
#include "services/database/User.hpp"

TmpDatabase::TmpDatabase()
: _tmpFile {std::tmpnam(nullptr)}
, _fileDeleter {_tmpFile}
, _db {_tmpFile}
{
}

Database::Db&
TmpDatabase::getDb()
{
	return _db;
}

DatabaseFixture::~DatabaseFixture()
{
	testDatabaseEmpty();
}

void
DatabaseFixture::SetUpTestCase()
{
	_tmpDb = std::make_unique<TmpDatabase>();
	{
		Database::Session s {_tmpDb->getDb()};
		s.prepareTables();
		s.analyze();

		// remove default created entries
		{
			auto transaction {s.createUniqueTransaction()};
			auto clusterTypes {Database::ClusterType::find(s, Database::Range {})};
			for (auto clusterTypeId : clusterTypes.results)
			{
				auto clusterType {Database::ClusterType::find(s, clusterTypeId)};
				clusterType.remove();
			}
		}
	}
}

void
DatabaseFixture::TearDownTestCase()
{
	_tmpDb.reset();
}

void
DatabaseFixture::testDatabaseEmpty()
{
	using namespace Database;

	auto uniqueTransaction {session.createUniqueTransaction()};

	EXPECT_EQ(Artist::getCount(session), 0);
	EXPECT_EQ(Cluster::getCount(session), 0);
	EXPECT_EQ(ClusterType::getCount(session), 0);
	EXPECT_EQ(Listen::getCount(session), 0);
	EXPECT_EQ(Release::getCount(session), 0);
	EXPECT_EQ(StarredArtist::getCount(session), 0);
	EXPECT_EQ(StarredRelease::getCount(session), 0);
	EXPECT_EQ(StarredTrack::getCount(session), 0);
	EXPECT_EQ(Track::getCount(session), 0);
	EXPECT_EQ(TrackBookmark::getCount(session), 0);
	EXPECT_EQ(TrackList::getCount(session), 0);
	EXPECT_EQ(User::getCount(session), 0);
}

TEST_F(DatabaseFixture, Common_subRangeEmpty)
{
	using namespace Database;

	RangeResults<int> results;
	results.range = Range {0, 0};
	results.results = {};
	results.moreResults = false;

	{
		auto subRange {results.getSubRange(Range {0, 0})};
		EXPECT_FALSE(subRange.moreResults);
		ASSERT_EQ(subRange.results.size(), 0);
		EXPECT_EQ(subRange.range, Range {});
	}
	{
		auto subRange {results.getSubRange(Range {0, 1})};
		EXPECT_FALSE(subRange.moreResults);
		ASSERT_EQ(subRange.results.size(), 0);
	}
}

TEST_F(DatabaseFixture, Common_IdType)
{
	using namespace Database;

	{
		const IdType id{};
		EXPECT_FALSE(id.isValid());
	}

	{
		const IdType id{0};
		EXPECT_TRUE(id.isValid());
	}

	{
		const IdType id1{0};
		const IdType id2{0};
		EXPECT_EQ(id1, id2);
	}

	{
		const IdType id1{0};
		const IdType id2{1};
		EXPECT_NE(id1, id2);
		EXPECT_LT(id1, id2);
		EXPECT_GT(id2, id1);
	}
}

TEST_F(DatabaseFixture, Common_subRange)
{
	using namespace Database;

	RangeResults<int> results;
	results.range = Range {0, 2};
	results.results = {5, 6};
	results.moreResults = false;

	{
		auto subRange {results.getSubRange(Range {0, 1})};
		EXPECT_TRUE(subRange.moreResults);
		ASSERT_EQ(subRange.results.size(), 1);
		EXPECT_EQ(subRange.results.front(), 5);
	}
	{
		auto subRange {results.getSubRange(Range {1, 1})};
		EXPECT_FALSE(subRange.moreResults);
		ASSERT_EQ(subRange.results.size(), 1);
		EXPECT_EQ(subRange.results.front(), 6);
	}
	{
		auto subRange {results.getSubRange(Range {0, 2})};
		EXPECT_FALSE(subRange.moreResults);
		ASSERT_EQ(subRange.results.size(), 2);
		EXPECT_EQ(subRange.results.front(), 5);
		EXPECT_EQ(subRange.results.back(), 6);
	}
	{
		auto subRange {results.getSubRange(Range {})};
		EXPECT_FALSE(subRange.moreResults);
		ASSERT_EQ(subRange.results.size(), 2);
		EXPECT_EQ(subRange.results.front(), 5);
		EXPECT_EQ(subRange.results.back(), 6);
		EXPECT_EQ(subRange.range, results.range);
	}

	{
		auto subRange {results.getSubRange(Range {1, 0})};
		EXPECT_FALSE(subRange.moreResults);
		ASSERT_EQ(subRange.results.size(), 1);
		EXPECT_EQ(subRange.results.front(), 6);
		const Range expectedRange {1, 1};
		EXPECT_EQ(subRange.range, expectedRange);
	}
	{
		auto subRange {results.getSubRange(Range {3, 2})};
		EXPECT_FALSE(subRange.moreResults);
		ASSERT_EQ(subRange.results.size(), 0);
		const Range expectedRange {2, 0};
		EXPECT_EQ(subRange.range, expectedRange);
	}
}

