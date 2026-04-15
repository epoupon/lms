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

#include "core/math/StatsAccumulator.hpp"

namespace lms::core::math::statsAccumulatorTests
{
    constexpr double epsilon{ 1e-4 };

    TEST(StatsAccumulator, initialState)
    {
        StatsAccumulator stats;

        EXPECT_EQ(stats.getCount(), 0);
        EXPECT_DOUBLE_EQ(stats.getMean(), 0.0);
        EXPECT_DOUBLE_EQ(stats.getPopulationVariance(), 0.0);
        EXPECT_DOUBLE_EQ(stats.getSampleVariance(), 0.0);
        EXPECT_DOUBLE_EQ(stats.getPopulationStdDev(), 0.0);
        EXPECT_DOUBLE_EQ(stats.getSampleSkewness(), 0.0);
    }

    TEST(StatsAccumulator, singleValue)
    {
        StatsAccumulator stats;
        stats.add(5.0);

        EXPECT_EQ(stats.getCount(), 1);
        EXPECT_DOUBLE_EQ(stats.getMean(), 5.0);

        // Variance should be 0 for a single value
        EXPECT_DOUBLE_EQ(stats.getPopulationVariance(), 0.0);
        EXPECT_DOUBLE_EQ(stats.getSampleVariance(), 0.0);
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
        EXPECT_NEAR(stats.getPopulationVariance(), 2.6667, epsilon);
    }

    TEST(StatsAccumulator, sampleVariance)
    {
        StatsAccumulator stats;
        stats.add(2.0);
        stats.add(4.0);
        stats.add(6.0);

        // Sample variance = 8 / 2 = 4
        EXPECT_NEAR(stats.getSampleVariance(), 4.0, epsilon);
    }

    TEST(StatsAccumulator, standardDeviation)
    {
        StatsAccumulator stats;
        stats.add(2.0);
        stats.add(4.0);
        stats.add(6.0);

        // sqrt(4) = 2 (sample stddev)
        EXPECT_NEAR(stats.getSampleStdDev(), 2.0, epsilon);
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
        EXPECT_NEAR(stats.getSampleSkewness(), 0.0, 0.5);
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
        EXPECT_GT(stats.getSampleSkewness(), epsilon);
    }

    TEST(StatsAccumulator, skewnessNeedsAtLeastThreeSamples)
    {
        StatsAccumulator stats;
        stats.add(2.0);
        stats.add(4.0);

        EXPECT_DOUBLE_EQ(stats.getSampleSkewness(), 0.0);
    }

    TEST(StatsAccumulator, skewnessConstantData)
    {
        StatsAccumulator stats;
        stats.add(3.0);
        stats.add(3.0);
        stats.add(3.0);
        stats.add(3.0);

        EXPECT_DOUBLE_EQ(stats.getSampleSkewness(), 0.0);
    }

    TEST(StatsAccumulator, largeMagnitudeValues)
    {
        // Welford's algorithm must stay numerically stable with large inputs
        constexpr double big{ 1e12 };
        StatsAccumulator stats;
        stats.add(big);
        stats.add(big + 1.0);
        stats.add(big + 2.0);

        EXPECT_NEAR(stats.getMean(), big + 1.0, 1e-6);
        EXPECT_NEAR(stats.getSampleVariance(), 1.0, 1e-6);
        EXPECT_NEAR(stats.getSampleStdDev(), 1.0, 1e-6);
    }

    TEST(StatsAccumulator, negativeValues)
    {
        StatsAccumulator stats;
        stats.add(-6.0);
        stats.add(-4.0);
        stats.add(-2.0);

        EXPECT_NEAR(stats.getMean(), -4.0, epsilon);
        EXPECT_NEAR(stats.getSampleVariance(), 4.0, epsilon);
    }

    TEST(StatsAccumulator, mixedSignValues)
    {
        StatsAccumulator stats;
        stats.add(-1.0);
        stats.add(0.0);
        stats.add(1.0);

        EXPECT_NEAR(stats.getMean(), 0.0, epsilon);
        EXPECT_NEAR(stats.getSampleVariance(), 1.0, epsilon);
    }

    TEST(StatsAccumulator, smallMagnitudeValues)
    {
        // Values close to double subnormal range; variance must stay non-negative
        constexpr double tiny{ 1e-300 };
        StatsAccumulator stats;
        stats.add(tiny);
        stats.add(tiny * 2.0);
        stats.add(tiny * 3.0);

        EXPECT_GE(stats.getSampleVariance(), 0.0);
        EXPECT_GE(stats.getSampleStdDev(), 0.0);
    }
} // namespace lms::core::math::statsAccumulatorTests