/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <gtest/gtest.h>

#include "services/scanner/ScannerStats.hpp"

namespace lms::scanner::tests
{
    TEST(ScannerStats, totalFileCount)
    {
        ScanStats stats;
        stats.skips = 5;
        stats.additions = 3;
        stats.updates = 7;
        stats.failures = 2;

        EXPECT_EQ(stats.getTotalFileCount(), 17U);
    }

    TEST(ScannerStats, changesCount)
    {
        ScanStats stats;
        stats.additions = 4;
        stats.deletions = 6;
        stats.updates = 8;

        EXPECT_EQ(stats.getChangesCount(), 18U);
    }

    TEST(ScannerStats, progressWithZeroTotal)
    {
        ScanStepStats stepStats;
        stepStats.totalElems = 0;
        stepStats.processedElems = 0;

        EXPECT_EQ(stepStats.progress(), 0U);
    }

    TEST(ScannerStats, progressAt100Percent)
    {
        ScanStepStats stepStats;
        stepStats.totalElems = 50;
        stepStats.processedElems = 50;

        EXPECT_EQ(stepStats.progress(), 100U);
    }

    TEST(ScannerStats, progressCanExceed100Percent)
    {
        ScanStepStats stepStats;
        stepStats.totalElems = 2;
        stepStats.processedElems = 5;

        EXPECT_EQ(stepStats.progress(), 250U);
    }
} // namespace lms::scanner::tests
