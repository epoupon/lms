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

#include <cmath>

#include <gtest/gtest.h>

#include "math/ChamferDistance.hpp"
#include "math/Vector.hpp"

namespace lms::math::chamferDistanceTests
{
    constexpr float epsilon{ 1e-4F };

    template<std::size_t Size>
    struct SimpleDistance
    {
        SimpleDistance(const Vector<Size, float>& ref)
            : _ref{ ref } {}

        float operator()(const Vector<Size, float>& b) const
        {
            float sum{};
            for (std::size_t i{}; i < Size; ++i)
            {
                const float diff{ _ref[i] - b[i] };
                sum += diff * diff;
            }
            return std::sqrt(sum);
        }

        const Vector<Size, float>& _ref;
    };

    TEST(ChamferDistance, singleElementSets)
    {
        const Vector<2, float> A[]{ { 0.F, 0.F } };
        const Vector<2, float> B[]{ { 3.F, 4.F } };

        const float result{ chamferDistanceAtoB<SimpleDistance<2>>(A, B) };

        const float expected{ 5.F }; // sqrt(3^2 + 4^2) = 5
        EXPECT_NEAR(result, expected, epsilon);
    }

    TEST(ChamferDistance, identicalSets)
    {
        const Vector<2, float> A[]{ { 1.F, 2.F }, { 3.F, 4.F } };

        const float result{ chamferDistanceAtoB<SimpleDistance<2>>(A, A) };

        EXPECT_NEAR(result, 0.F, epsilon);
    }

    TEST(ChamferDistance, asymmetricDistance)
    {
        // A = {(0,0), (1,0)}, B = {(0,0), (2,0)}
        // For a=(0,0): min(dist to (0,0), dist to (2,0)) = 0
        // For a=(1,0): min(dist to (0,0), dist to (2,0)) = min(1, 1) = 1
        // Average = (0 + 1) / 2 = 0.5

        const Vector<2, float> A[]{ { 0.F, 0.F }, { 1.F, 0.F } };
        const Vector<2, float> B[]{ { 0.F, 0.F }, { 2.F, 0.F } };

        const float result{ chamferDistanceAtoB<SimpleDistance<2>>(A, B) };

        EXPECT_NEAR(result, 0.5F, epsilon);
    }

    TEST(ChamferDistance, symmetricalDistance)
    {
        const Vector<2, float> A[]{ { 0.F, 0.F }, { 2.F, 0.F } };
        const Vector<2, float> B[]{ { 0.F, 0.F }, { 1.F, 0.F } };

        const float symDist{ symmetricalChamferDistance<SimpleDistance<2>>(A, B) };

        const float aToB{ chamferDistanceAtoB<SimpleDistance<2>>(A, B) };
        const float bToA{ chamferDistanceAtoB<SimpleDistance<2>>(B, A) };
        const float expected{ (aToB + bToA) / 2.F };

        EXPECT_NEAR(symDist, expected, epsilon);
    }

    TEST(ChamferDistance, largerSets)
    {
        // A has 3 elements, B has 2 elements
        const Vector<2, float> A[]{ { 0.F, 0.F }, { 1.F, 1.F }, { 2.F, 2.F } };
        const Vector<2, float> B[]{ { 0.F, 0.F }, { 3.F, 3.F } };

        const float result{ chamferDistanceAtoB<SimpleDistance<2>>(A, B) };

        // a1: min(0, sqrt(27)) = 0
        // a2: min(sqrt(2), sqrt(8)) = sqrt(2)
        // a3: min(sqrt(8), sqrt(2)) = sqrt(2)
        // Average = (0 + sqrt(2) + sqrt(2)) / 3 = 2*sqrt(2) / 3
        const float expected{ 2.F * std::sqrt(2.F) / 3.F };

        EXPECT_NEAR(result, expected, epsilon);
    }

    TEST(ChamferDistance, negativeCoordinates)
    {
        const Vector<2, float> A[]{ { -1.F, -1.F } };
        const Vector<2, float> B[]{ { 1.F, 1.F } };

        const float result{ chamferDistanceAtoB<SimpleDistance<2>>(A, B) };

        const float expected{ std::sqrt(8.F) }; // sqrt(2^2 + 2^2)
        EXPECT_NEAR(result, expected, epsilon);
    }

    TEST(ChamferDistance, higherDimensions)
    {
        const Vector<5, float> A[]{ { 1.F, 2.F, 3.F, 4.F, 5.F } };
        const Vector<5, float> B[]{ { 1.F, 2.F, 3.F, 4.F, 5.F } };

        const float result{ chamferDistanceAtoB<SimpleDistance<5>>(A, B) };

        EXPECT_NEAR(result, 0.F, epsilon);
    }
} // namespace lms::math::chamferDistanceTests
