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

#include "features/StatsAccumulator.hpp"

namespace lms::audio::features::statsAccumulatorTests
{
    constexpr float epsilon{ 1e-4F };

    TEST(StatsAccumulator, initialState)
    {
        StatsAccumulator stats;

        EXPECT_EQ(stats.getCount(), 0);
        EXPECT_FLOAT_EQ(stats.getMean(), 0.F);
        EXPECT_FLOAT_EQ(stats.getVariance(StatsAccumulator::Sample{ false }), 0.F);
        EXPECT_FLOAT_EQ(stats.getVariance(StatsAccumulator::Sample{ true }), 0.F);
        EXPECT_FLOAT_EQ(stats.getStdDev(StatsAccumulator::Sample{ false }), 0.F);
        EXPECT_FLOAT_EQ(stats.getSkewness(), 0.F);
    }

    TEST(StatsAccumulator, singleValue)
    {
        StatsAccumulator stats;
        stats.add(5.0);

        EXPECT_EQ(stats.getCount(), 1);
        EXPECT_FLOAT_EQ(stats.getMean(), 5.F);

        // Variance should be 0 for a single value
        EXPECT_FLOAT_EQ(stats.getVariance(StatsAccumulator::Sample{ false }), 0.F);
        EXPECT_FLOAT_EQ(stats.getVariance(StatsAccumulator::Sample{ true }), 0.F);
    }

    TEST(StatsAccumulator, multipleValuesMean)
    {
        StatsAccumulator stats;
        stats.add(2.0);
        stats.add(4.0);
        stats.add(6.0);

        EXPECT_EQ(stats.getCount(), 3);
        EXPECT_FLOAT_EQ(stats.getMean(), 4.F);
    }

    TEST(StatsAccumulator, populationVariance)
    {
        StatsAccumulator stats;
        stats.add(2.0);
        stats.add(4.0);
        stats.add(6.0);

        // Population variance = 8 / 3 ≈ 2.6667
        EXPECT_NEAR(stats.getVariance(), 2.6667F, epsilon);
    }

    TEST(StatsAccumulator, sampleVariance)
    {
        StatsAccumulator stats;
        stats.add(2.0);
        stats.add(4.0);
        stats.add(6.0);

        // Sample variance = 8 / 2 = 4
        EXPECT_NEAR(stats.getVariance(StatsAccumulator::Sample{ true }), 4.F, epsilon);
    }

    TEST(StatsAccumulator, standardDeviation)
    {
        StatsAccumulator stats;
        stats.add(2.0);
        stats.add(4.0);
        stats.add(6.0);

        // sqrt(4) = 2 (sample stddev)
        EXPECT_NEAR(stats.getStdDev(StatsAccumulator::Sample{ true }), 2.F, epsilon);
    }

    TEST(StatsAccumulator, skewnessSymmetricData)
    {
        StatsAccumulator stats;
        stats.add(1.0);
        stats.add(2.0);
        stats.add(3.0);
        stats.add(4.0);
        stats.add(5.0);

        // Symmetric distribution → skewness ≈ 0
        EXPECT_NEAR(stats.getSkewness(), 0.F, 0.5F);
    }

    TEST(StatsAccumulator, skewnessAsymmetricData)
    {
        StatsAccumulator stats;
        stats.add(1.0);
        stats.add(2.0);
        stats.add(3.0);
        stats.add(4.0);
        stats.add(10.0);

        // Should be positively skewed
        EXPECT_GT(stats.getSkewness(), epsilon);
    }
} // namespace lms::audio::features::statsAccumulatorTests