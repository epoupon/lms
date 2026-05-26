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

#include "math/NormalizedCosineDistance.hpp"

namespace lms::math::normalizedCosineDistanceTests
{
    constexpr float epsilon{ 1e-6F };

    TEST(NormalizedCosineDistance, equalNormalizedVectors)
    {
        Vector<3, float> a{ 1.F, 2.F, 3.F };
        Vector<3, float> b{ 1.F, 2.F, 3.F };

        a.normalizeL2();
        b.normalizeL2();

        EXPECT_NEAR(computeNormalizedCosineDistance(a, b), 0.F, epsilon);
    }

    TEST(NormalizedCosineDistance, orthogonalNormalizedVectors)
    {
        Vector<3, float> a{ 1.F, 0.F, 0.F };
        Vector<3, float> b{ 0.F, 1.F, 0.F };

        a.normalizeL2();
        b.normalizeL2();

        EXPECT_NEAR(computeNormalizedCosineDistance(a, b), 0.5F, epsilon);
    }

    TEST(NormalizedCosineDistance, oppositeNormalizedVectors)
    {
        Vector<3, float> a{ 1.F, 1.F, 0.F };
        Vector<3, float> b{ -1.F, -1.F, 0.F };

        a.normalizeL2();
        b.normalizeL2();

        EXPECT_NEAR(computeNormalizedCosineDistance(a, b), 1.F, epsilon);
    }

    TEST(NormalizedCosineDistance, functor)
    {
        Vector<3, float> reference{ 1.F, 0.F, 0.F };
        Vector<3, float> candidate{ 0.F, 1.F, 0.F };

        reference.normalizeL2();
        candidate.normalizeL2();

        const NormalizedCosineDistance<3, float> distance{ reference };

        EXPECT_NEAR(distance(candidate), 0.5F, epsilon);
    }
} // namespace lms::math::normalizedCosineDistanceTests
