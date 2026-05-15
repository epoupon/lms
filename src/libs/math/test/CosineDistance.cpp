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

#include "math/CosineDistance.hpp"

namespace lms::math::cosineDistanceTests
{
    constexpr float epsilon{ 1e-6F };

    TEST(CosineDistance, equalVectors)
    {
        const Vector<3, float> a{ 1.F, 2.F, 3.F };
        const Vector<3, float> b{ 1.F, 2.F, 3.F };

        EXPECT_NEAR(computeCosineDistance(a, b), 0.F, epsilon);
    }

    TEST(CosineDistance, orthogonalVectors)
    {
        const Vector<3, float> a{ 1.F, 0.F, 0.F };
        const Vector<3, float> b{ 0.F, 1.F, 0.F };

        EXPECT_NEAR(computeCosineDistance(a, b), 1.F, epsilon);
    }

    TEST(CosineDistance, oppositeVectors)
    {
        const Vector<3, float> a{ 1.F, 2.F, 3.F };
        const Vector<3, float> b{ -1.F, -2.F, -3.F };

        EXPECT_NEAR(computeCosineDistance(a, b), 2.F, epsilon);
    }

    TEST(CosineDistance, zeroNormVector)
    {
        const Vector<3, float> a{ 0.F, 0.F, 0.F };
        const Vector<3, float> b{ 1.F, 2.F, 3.F };

        EXPECT_FLOAT_EQ(computeCosineDistance(a, b), 1.F);
    }

    TEST(CosineDistance, vectorMethod)
    {
        const Vector<3, float> a{ 1.F, 2.F, 3.F };
        const Vector<3, float> b{ 1.F, 2.F, 3.F };

        EXPECT_NEAR(computeCosineDistance(a, b), 0.F, epsilon);
    }

    TEST(CosineDistance, functor)
    {
        const Vector<3, float> reference{ 1.F, 0.F, 0.F };
        const Vector<3, float> candidate{ 0.F, 1.F, 0.F };
        const CosineDistance<3, float> distance{ reference };

        EXPECT_NEAR(distance(candidate), 1.F, epsilon);
    }
} // namespace lms::math::cosineDistanceTests