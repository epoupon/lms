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

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Image.hpp"
#include "database/Listen.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/StarredArtist.hpp"
#include "database/StarredRelease.hpp"
#include "database/StarredTrack.hpp"
#include "database/Track.hpp"
#include "database/TrackArtistLink.hpp"
#include "database/TrackBookmark.hpp"
#include "database/TrackList.hpp"
#include "database/Types.hpp"
#include "database/User.hpp"

namespace lms::db::tests
{
    TmpDatabase::TmpDatabase()
        : _tmpFile{ std::tmpnam(nullptr) }
        , _fileDeleter{ _tmpFile }
        , _db{ _tmpFile }
    {
    }

    db::Db& TmpDatabase::getDb()
    {
        return _db;
    }

    DatabaseFixture::~DatabaseFixture()
    {
        testDatabaseEmpty();
    }

    void DatabaseFixture::SetUpTestCase()
    {
        _tmpDb = std::make_unique<TmpDatabase>();
        {
            db::Session s{ _tmpDb->getDb() };
            s.prepareTablesIfNeeded();
            s.createIndexesIfNeeded();
        }
    }

    void DatabaseFixture::TearDownTestCase()
    {
        _tmpDb.reset();
    }

    void DatabaseFixture::testDatabaseEmpty()
    {
        using namespace db;

        auto transaction{ session.createWriteTransaction() };

        EXPECT_EQ(Artist::getCount(session), 0);
        EXPECT_EQ(Cluster::getCount(session), 0);
        EXPECT_EQ(ClusterType::getCount(session), 0);
        EXPECT_EQ(Label::getCount(session), 0);
        EXPECT_EQ(Listen::getCount(session), 0);
        EXPECT_EQ(Image::getCount(session), 0);
        EXPECT_EQ(MediaLibrary::getCount(session), 0);
        EXPECT_EQ(Release::getCount(session), 0);
        EXPECT_EQ(ReleaseType::getCount(session), 0);
        EXPECT_EQ(StarredArtist::getCount(session), 0);
        EXPECT_EQ(StarredRelease::getCount(session), 0);
        EXPECT_EQ(StarredTrack::getCount(session), 0);
        EXPECT_EQ(Track::getCount(session), 0);
        EXPECT_EQ(TrackBookmark::getCount(session), 0);
        EXPECT_EQ(TrackList::getCount(session), 0);
        EXPECT_EQ(User::getCount(session), 0);
    }

    TEST_F(DatabaseFixture, vacuum)
    {
        session.vacuum();
    }

    TEST_F(DatabaseFixture, analyze)
    {
        session.fullAnalyze();
    }

    TEST_F(DatabaseFixture, Common_subRangeEmpty)
    {
        RangeResults<int> results;
        results.range = Range{ 0, 0 };
        results.results = {};
        results.moreResults = false;

        {
            auto subRange{ results.getSubRange(Range{ 0, 0 }) };
            EXPECT_FALSE(subRange.moreResults);
            ASSERT_EQ(subRange.results.size(), 0);
            EXPECT_EQ(subRange.range, Range{});
        }
        {
            auto subRange{ results.getSubRange(Range{ 0, 1 }) };
            EXPECT_FALSE(subRange.moreResults);
            ASSERT_EQ(subRange.results.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Common_subRangeForeach)
    {
        struct TestCase
        {
            Range range;
            std::size_t subRangeSize;
            std::vector<Range> expectedSubRanges;
        };

        TestCase testCases[]{
            { Range{ 0, 0 }, 1, {} },
            { Range{ 1, 0 }, 1, {} },
            { Range{ 1, 1 }, 1, { Range{ 1, 1 } } },
            { Range{ 1, 3 }, 1, { Range{ 1, 1 }, Range{ 2, 1 }, Range{ 3, 1 } } },
            { Range{ 0, 100 }, 100, { Range{ 0, 100 } } },
            { Range{ 0, 50 }, 100, { Range{ 0, 50 } } },
            { Range{ 100, 200 }, 100, { Range{ 100, 100 }, Range{ 200, 100 } } },
            { Range{ 100, 101 }, 100, { Range{ 100, 100 }, Range{ 200, 1 } } },
            { Range{ 1000, 10 }, 100, { Range{ 1000, 10 } } },
            { Range{ 1, 100 }, 50, { Range{ 1, 50 }, Range{ 51, 50 } } },
        };

        for (const TestCase& test : testCases)
        {
            std::vector<Range> subRanges;
            foreachSubRange(test.range, test.subRangeSize, [&](Range subRange) {
                subRanges.push_back(subRange);
                return true;
            });

            EXPECT_EQ(subRanges, test.expectedSubRanges) << ", test index = " << std::distance(std::cbegin(testCases), &test);
        }
    }

    TEST_F(DatabaseFixture, Common_IdType)
    {
        {
            const IdType id{};
            EXPECT_FALSE(id.isValid());
        }

        {
            const IdType id{ 0 };
            EXPECT_TRUE(id.isValid());
        }

        {
            const IdType id1{ 0 };
            const IdType id2{ 0 };
            EXPECT_EQ(id1, id2);
        }

        {
            const IdType id1{ 0 };
            const IdType id2{ 1 };
            EXPECT_NE(id1, id2);
            EXPECT_LT(id1, id2);
            EXPECT_GT(id2, id1);
        }
    }

    TEST_F(DatabaseFixture, Common_subRange)
    {
        RangeResults<int> results;
        results.range = Range{ 0, 2 };
        results.results = { 5, 6 };
        results.moreResults = false;

        {
            auto subRange{ results.getSubRange(Range{ 0, 1 }) };
            EXPECT_TRUE(subRange.moreResults);
            ASSERT_EQ(subRange.results.size(), 1);
            EXPECT_EQ(subRange.results.front(), 5);
        }
        {
            auto subRange{ results.getSubRange(Range{ 1, 1 }) };
            EXPECT_FALSE(subRange.moreResults);
            ASSERT_EQ(subRange.results.size(), 1);
            EXPECT_EQ(subRange.results.front(), 6);
        }
        {
            auto subRange{ results.getSubRange(Range{ 0, 2 }) };
            EXPECT_FALSE(subRange.moreResults);
            ASSERT_EQ(subRange.results.size(), 2);
            EXPECT_EQ(subRange.results.front(), 5);
            EXPECT_EQ(subRange.results.back(), 6);
        }
        {
            auto subRange{ results.getSubRange(Range{}) };
            EXPECT_FALSE(subRange.moreResults);
            ASSERT_EQ(subRange.results.size(), 2);
            EXPECT_EQ(subRange.results.front(), 5);
            EXPECT_EQ(subRange.results.back(), 6);
            EXPECT_EQ(subRange.range, results.range);
        }

        {
            auto subRange{ results.getSubRange(Range{ 1, 0 }) };
            EXPECT_FALSE(subRange.moreResults);
            ASSERT_EQ(subRange.results.size(), 1);
            EXPECT_EQ(subRange.results.front(), 6);
            const Range expectedRange{ 1, 1 };
            EXPECT_EQ(subRange.range, expectedRange);
        }
        {
            auto subRange{ results.getSubRange(Range{ 3, 2 }) };
            EXPECT_FALSE(subRange.moreResults);
            ASSERT_EQ(subRange.results.size(), 0);
            const Range expectedRange{ 2, 0 };
            EXPECT_EQ(subRange.range, expectedRange);
        }
    }
} // namespace lms::db::tests