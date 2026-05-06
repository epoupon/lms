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

#include "math/StatsAccumulator.hpp"

namespace lms::math::statsAccumulatorTests
{
    constexpr float epsilon{ 1e-4F };

    TEST(StatsAccumulator, initialState)
    {
        StatsAccumulator stats;

        EXPECT_EQ(stats.getCount(), 0);
        EXPECT_FLOAT_EQ(stats.getMean(), 0.F);
        EXPECT_FLOAT_EQ(stats.getPopulationVariance(), 0.F);
        EXPECT_FLOAT_EQ(stats.getSampleVariance(), 0.F);
        EXPECT_FLOAT_EQ(stats.getPopulationStdDev(), 0.F);
    }

    TEST(StatsAccumulator, singleValue)
    {
        constexpr float value{ 5.F };
        StatsAccumulator stats;
        stats.add(value);

        EXPECT_EQ(stats.getCount(), 1);
        EXPECT_FLOAT_EQ(stats.getMean(), value);

        // Variance should be 0 for a single value
        EXPECT_FLOAT_EQ(stats.getPopulationVariance(), 0.F);
        EXPECT_FLOAT_EQ(stats.getSampleVariance(), 0.F);
    }

    TEST(StatsAccumulator, multipleValuesMean)
    {
        constexpr float a{ 2.F };
        constexpr float b{ 4.F };
        constexpr float c{ 6.F };
        StatsAccumulator stats;
        stats.add(a);
        stats.add(b);
        stats.add(c);

        EXPECT_EQ(stats.getCount(), 3);
        EXPECT_FLOAT_EQ(stats.getMean(), b);
    }

    TEST(StatsAccumulator, populationVariance)
    {
        constexpr float a{ 2.F };
        constexpr float b{ 4.F };
        constexpr float c{ 6.F };
        constexpr float expectedVariance{ 8.F / 3.F };
        StatsAccumulator stats;
        stats.add(a);
        stats.add(b);
        stats.add(c);

        // Population variance = 8 / 3 ≈ 2.6667
        EXPECT_NEAR(stats.getPopulationVariance(), expectedVariance, epsilon);
    }

    TEST(StatsAccumulator, sampleVariance)
    {
        constexpr float a{ 2.F };
        constexpr float b{ 4.F };
        constexpr float c{ 6.F };
        constexpr float expectedVariance{ 4.F };
        StatsAccumulator stats;
        stats.add(a);
        stats.add(b);
        stats.add(c);

        // Sample variance = 8 / 2 = 4
        EXPECT_NEAR(stats.getSampleVariance(), expectedVariance, epsilon);
    }

    TEST(StatsAccumulator, standardDeviation)
    {
        constexpr float a{ 2.F };
        constexpr float b{ 4.F };
        constexpr float c{ 6.F };
        constexpr float expectedStdDev{ 2.F };
        StatsAccumulator stats;
        stats.add(a);
        stats.add(b);
        stats.add(c);

        // sqrt(4) = 2 (sample stddev)
        EXPECT_NEAR(stats.getSampleStdDev(), expectedStdDev, epsilon);
    }

    TEST(StatsAccumulator, largeMagnitudeValues)
    {
        // Welford's algorithm must stay numerically stable with large inputs
        // 1e6 is within float's ~7 significant-digit range
        constexpr float big{ 1e6F };
        constexpr float offset{ 2.F };
        StatsAccumulator stats;
        stats.add(big);
        stats.add(big + 1.F);
        stats.add(big + offset);

        EXPECT_NEAR(stats.getMean(), big + 1.F, 1e-1F);
        EXPECT_NEAR(stats.getSampleVariance(), 1.F, 1e-1F);
        EXPECT_NEAR(stats.getSampleStdDev(), 1.F, 1e-1F);
    }

    TEST(StatsAccumulator, negativeValues)
    {
        constexpr float a{ -6.F };
        constexpr float b{ -4.F };
        constexpr float c{ -2.F };
        constexpr float expectedVariance{ 4.F };
        StatsAccumulator stats;
        stats.add(a);
        stats.add(b);
        stats.add(c);

        EXPECT_NEAR(stats.getMean(), b, epsilon);
        EXPECT_NEAR(stats.getSampleVariance(), expectedVariance, epsilon);
    }

    TEST(StatsAccumulator, mixedSignValues)
    {
        constexpr float a{ -1.F };
        constexpr float b{ 0.F };
        constexpr float c{ 1.F };
        constexpr float expectedVariance{ 1.F };
        StatsAccumulator stats;
        stats.add(a);
        stats.add(b);
        stats.add(c);

        EXPECT_NEAR(stats.getMean(), 0.F, epsilon);
        EXPECT_NEAR(stats.getSampleVariance(), expectedVariance, epsilon);
    }

    TEST(StatsAccumulator, smallMagnitudeValues)
    {
        // Values well within float's normal range; variance must stay non-negative
        constexpr float tiny{ 1e-30F };
        constexpr float multiplier2{ 2.F };
        constexpr float multiplier3{ 3.F };
        StatsAccumulator stats;
        stats.add(tiny);
        stats.add(tiny * multiplier2);
        stats.add(tiny * multiplier3);

        EXPECT_GE(stats.getSampleVariance(), 0.F);
        EXPECT_GE(stats.getSampleStdDev(), 0.F);
    }
} // namespace lms::math::statsAccumulatorTests