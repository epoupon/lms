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

#include <array>

#include <gtest/gtest.h>

#include "core/math/EuclideanDistance.hpp"

namespace lms::core::math::euclideanDistanceTests
{
    constexpr float epsilon{ 1e-4F };

    TEST(EuclideanDistance, zeroLength)
    {
        const std::array<float, 0> a{};
        const std::array<float, 0> b{};
        const std::array<float, 0> weights{};

        EXPECT_FLOAT_EQ(computeEuclideanSquaredDistance(a.data(), b.data(), 0U), 0.F);
        EXPECT_FLOAT_EQ(computeEuclideanSquaredDistance(a.data(), b.data(), weights.data(), 0U), 0.F);
    }

    TEST(EuclideanDistance, equalVectors)
    {
        const std::array<float, 3> a{ 1.F, 2.F, 3.F };
        const std::array<float, 3> b{ 1.F, 2.F, 3.F };

        EXPECT_FLOAT_EQ(computeEuclideanSquaredDistance(a.data(), b.data(), a.size()), 0.F);
    }

    TEST(EuclideanDistance, unweightedDistance)
    {
        const std::array<float, 3> a{ 1.F, 2.F, 3.F };
        const std::array<float, 3> b{ 4.F, 6.F, 8.F };

        const float expected{ 50.F }; // 3^2 + 4^2 + 5^2
        EXPECT_NEAR(computeEuclideanSquaredDistance(a.data(), b.data(), a.size()), expected, epsilon);
    }

    TEST(EuclideanDistance, weightedDistance)
    {
        const std::array<float, 3> a{ 1.F, 3.F, 5.F };
        const std::array<float, 3> b{ 2.F, 1.F, 6.F };
        const std::array<float, 3> weights{ 1.F, 0.5F, 2.F };

        const float expected{ 5.F }; // 1*1 + 4*0.5 + 1*2
        EXPECT_NEAR(computeEuclideanSquaredDistance(a.data(), b.data(), weights.data(), a.size()), expected, epsilon);
    }
} // namespace lms::core::math::euclideanDistanceTests