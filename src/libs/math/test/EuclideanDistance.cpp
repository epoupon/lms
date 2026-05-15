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

#include <limits>

#include <gtest/gtest.h>

#include "math/EuclideanDistance.hpp"

namespace lms::math::euclideanDistanceTests
{
    constexpr float epsilon{ 1e-4F };

    TEST(EuclideanDistance, zeroLength)
    {
        const Vector<0, float> a{};
        const Vector<0, float> b{};
        const Vector<0, float> weights{};

        EXPECT_FLOAT_EQ(computeEuclideanSquaredDistance(a, b), 0.F);
        EXPECT_FLOAT_EQ(computeEuclideanSquaredDistanceWithWeights(a, b, weights), 0.F);
    }

    TEST(EuclideanDistance, equalVectors)
    {
        const Vector<3, float> a{ 1.F, 2.F, 3.F };
        const Vector<3, float> b{ 1.F, 2.F, 3.F };

        EXPECT_FLOAT_EQ(computeEuclideanSquaredDistance(a, b), 0.F);
    }

    TEST(EuclideanDistance, unweightedDistance)
    {
        const Vector<3, float> a{ 1.F, 2.F, 3.F };
        const Vector<3, float> b{ 4.F, 6.F, 8.F };

        const float expected{ 50.F }; // 3^2 + 4^2 + 5^2
        EXPECT_NEAR(computeEuclideanSquaredDistance(a, b), expected, epsilon);
    }

    TEST(EuclideanDistance, weightedDistance)
    {
        const Vector<3, float> a{ 1.F, 3.F, 5.F };
        const Vector<3, float> b{ 2.F, 1.F, 6.F };
        const Vector<3, float> weights{ 1.F, 0.5F, 2.F };

        const float expected{ 5.F }; // 1*1 + 4*0.5 + 1*2
        EXPECT_NEAR(computeEuclideanSquaredDistanceWithWeights(a, b, weights), expected, epsilon);
    }

    TEST(EuclideanDistance, largeMagnitudeValues)
    {
        // 1e15^2 * 2 = 2e30, well within the float max (~3.4e38), so no overflow
        const float big{ 1e15F };
        const Vector<2, float> a{ big, big };
        const Vector<2, float> b{ 0.F, 0.F };

        const float result{ computeEuclideanSquaredDistance(a, b) };
        EXPECT_GT(result, 0.F);
    }

    TEST(EuclideanDistance, smallMagnitudeValues)
    {
        // Subnormal inputs; result must stay non-negative
        const float tiny{ std::numeric_limits<float>::min() };
        const Vector<3, float> a{ tiny, tiny, tiny };
        const Vector<3, float> b{ 0.F, 0.F, 0.F };

        const float result{ computeEuclideanSquaredDistance(a, b) };
        EXPECT_GE(result, 0.F);
    }

    TEST(EuclideanDistance, negativeValues)
    {
        // Negative components must produce the same result as their positive mirror
        const Vector<3, float> a{ -1.F, -2.F, -3.F };
        const Vector<3, float> b{ 1.F, 2.F, 3.F };
        const Vector<3, float> aMirror{ 1.F, 2.F, 3.F };
        const Vector<3, float> bMirror{ -1.F, -2.F, -3.F };

        EXPECT_FLOAT_EQ(
            computeEuclideanSquaredDistance(a, b),
            computeEuclideanSquaredDistance(aMirror, bMirror));
    }

    TEST(EuclideanDistance, zeroWeights)
    {
        const Vector<3, float> a{ 1.F, 2.F, 3.F };
        const Vector<3, float> b{ 4.F, 5.F, 6.F };
        const Vector<3, float> weights{ 0.F, 0.F, 0.F };

        EXPECT_FLOAT_EQ(computeEuclideanSquaredDistanceWithWeights(a, b, weights), 0.F);
    }

    TEST(EuclideanDistance, singleElement)
    {
        const Vector<1, float> a{ 3.F };
        const Vector<1, float> b{ 7.F };

        EXPECT_FLOAT_EQ(computeEuclideanSquaredDistance(a, b), 16.F);
    }
} // namespace lms::math::euclideanDistanceTests