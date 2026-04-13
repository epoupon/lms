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

#include "som/Vector.hpp"

namespace lms::som
{
    class VectorTest : public ::testing::Test
    {
    protected:
        static constexpr std::size_t VectorSize{ 3 };
        using TestVector = Vector<VectorSize, float>;
    };

    // Construction and basic properties
    TEST_F(VectorTest, ConstructorDefaultInitialization)
    {
        TestVector vec;
        EXPECT_EQ(vec.getSize(), VectorSize);
        for (std::size_t i{}; i < vec.getSize(); ++i)
            EXPECT_FLOAT_EQ(vec[i], 0.F);
    }

    TEST_F(VectorTest, ConstructorWithCustomValue)
    {
        TestVector vec{ 5.F };
        EXPECT_EQ(vec.getSize(), VectorSize);
        for (std::size_t i{}; i < vec.getSize(); ++i)
            EXPECT_FLOAT_EQ(vec[i], 5.F);
    }

    TEST_F(VectorTest, ConstructorWithCustomValues)
    {
        TestVector vec{ 5.F, 6.F, 7.F };
        EXPECT_EQ(vec.getSize(), VectorSize);
        EXPECT_FLOAT_EQ(vec[0], 5.F);
        EXPECT_FLOAT_EQ(vec[1], 6.F);
        EXPECT_FLOAT_EQ(vec[2], 7.F);
    }

    TEST_F(VectorTest, GetSize)
    {
        TestVector vec;
        EXPECT_EQ(vec.getSize(), 3);

        Vector<10, float> largeVec;
        EXPECT_EQ(largeVec.getSize(), 10);

        Vector<1, float> singleVec;
        EXPECT_EQ(singleVec.getSize(), 1);
    }

    TEST_F(VectorTest, ElementAccessMutable)
    {
        TestVector vec;
        vec[0] = 1.F;
        vec[1] = 2.F;
        vec[2] = 3.F;

        EXPECT_FLOAT_EQ(vec[0], 1.F);
        EXPECT_FLOAT_EQ(vec[1], 2.F);
        EXPECT_FLOAT_EQ(vec[2], 3.F);
    }

    TEST_F(VectorTest, ElementAccessConst)
    {
        TestVector vec{ 7.F };
        const auto& constVec{ vec };

        EXPECT_FLOAT_EQ(constVec[0], 7.F);
        EXPECT_FLOAT_EQ(constVec[1], 7.F);
        EXPECT_FLOAT_EQ(constVec[2], 7.F);
    }

    TEST_F(VectorTest, OperatorPlusEquals)
    {
        TestVector vec1{ 1.F };
        TestVector vec2{ 2.F };

        vec1 += vec2;

        for (std::size_t i{}; i < VectorSize; ++i)
            EXPECT_FLOAT_EQ(vec1[i], 3.F);
    }

    TEST_F(VectorTest, OperatorMinusEqualsNegative)
    {
        TestVector vec1{ 2.F };
        TestVector vec2{ 5.F };

        vec1 -= vec2;

        for (std::size_t i{}; i < VectorSize; ++i)
            EXPECT_FLOAT_EQ(vec1[i], -3.F);
    }

    TEST_F(VectorTest, OperatorMultiplyEquals)
    {
        TestVector vec{ 2.F };
        vec *= 3.F;

        for (std::size_t i{}; i < VectorSize; ++i)
            EXPECT_FLOAT_EQ(vec[i], 6.F);
    }

    // Norm computation
    TEST_F(VectorTest, ComputeNormZeroVector)
    {
        TestVector vec;
        EXPECT_FLOAT_EQ(vec.computeNorm(), 0.F);
    }

    TEST_F(VectorTest, ComputeNormSimple)
    {
        TestVector vec;
        vec[0] = 3.F;
        vec[1] = 4.F;
        vec[2] = 0.F;

        const float expectedNorm{ 5.F }; // 3-4-5 triangle
        EXPECT_FLOAT_EQ(vec.computeNorm(), expectedNorm);
    }

    TEST_F(VectorTest, ComputeNormUnitVectors)
    {
        TestVector vec{ 1.F };
        const float expectedNorm{ std::sqrt(static_cast<float>(VectorSize)) };
        EXPECT_FLOAT_EQ(vec.computeNorm(), expectedNorm);
    }

    TEST_F(VectorTest, ComputeNormNegativeValues)
    {
        TestVector vec;
        vec[0] = -3.F;
        vec[1] = -4.F;
        vec[2] = 0.F;

        const float expectedNorm{ 5.F };
        EXPECT_FLOAT_EQ(vec.computeNorm(), expectedNorm);
    }

    TEST_F(VectorTest, computeEuclideanSquaredDistanceIdentical)
    {
        TestVector vec1{ 1.F };
        TestVector vec2{ 1.F };

        EXPECT_FLOAT_EQ(vec1.computeEuclideanSquaredDistance(vec2), 0.F);
    }

    TEST_F(VectorTest, computeEuclideanSquaredDistanceSimple)
    {
        TestVector vec1;
        vec1[0] = 0.F;
        vec1[1] = 0.F;
        vec1[2] = 0.F;

        TestVector vec2;
        vec2[0] = 3.F;
        vec2[1] = 4.F;
        vec2[2] = 0.F;

        const float expectedDistance{ 25.F }; // (3-0)^2 + (4-0)^2 + (0-0)^2
        EXPECT_FLOAT_EQ(vec1.computeEuclideanSquaredDistance(vec2), expectedDistance);
        EXPECT_FLOAT_EQ(SquaredEuclideanDistance{ vec1 }(vec2), expectedDistance);
    }

    TEST_F(VectorTest, computeEuclideanSquaredDistanceWithWeights)
    {
        TestVector vec1{ 0.F };
        TestVector vec2{ 2.F };

        TestVector weights;
        weights[0] = 1.F;
        weights[1] = 2.F;
        weights[2] = 3.F;

        // (2-0)^2*1 + (2-0)^2*2 + (2-0)^2*3 = 4 + 8 + 12 = 24
        const float expectedDistance{ 24.F };
        EXPECT_FLOAT_EQ(vec1.computeEuclideanSquaredDistanceWithWeights(vec2, weights), expectedDistance);
        EXPECT_FLOAT_EQ((SquaredEuclideanDistanceWithWeights{ vec1, weights }(vec2)), expectedDistance);
    }

    TEST_F(VectorTest, computeEuclideanSquaredDistanceSymmetric)
    {
        TestVector vec1;
        vec1[0] = 1.F;
        vec1[1] = 2.F;
        vec1[2] = 3.F;

        TestVector vec2;
        vec2[0] = 4.F;
        vec2[1] = 5.F;
        vec2[2] = 6.F;

        {
            const float dist1{ vec1.computeEuclideanSquaredDistance(vec2) };
            const float dist2{ vec2.computeEuclideanSquaredDistance(vec1) };
            EXPECT_FLOAT_EQ(dist1, dist2);
        }

        {
            const float dist1{ SquaredEuclideanDistance{ vec1 }(vec2) };
            const float dist2{ SquaredEuclideanDistance{ vec2 }(vec1) };
            EXPECT_FLOAT_EQ(dist1, dist2);
        }
    }

    TEST_F(VectorTest, NormalizeL2ZeroVector)
    {
        TestVector vec;
        vec.normalizeL2();

        for (std::size_t i{}; i < VectorSize; ++i)
            EXPECT_FLOAT_EQ(vec[i], 0.F);
    }

    TEST_F(VectorTest, NormalizeL2Simple)
    {
        TestVector vec;
        vec[0] = 3.F;
        vec[1] = 4.F;
        vec[2] = 0.F;

        vec.normalizeL2();

        const float norm{ vec.computeNorm() };
        EXPECT_FLOAT_EQ(norm, 1.F);

        EXPECT_FLOAT_EQ(vec[0], 0.6F);
        EXPECT_FLOAT_EQ(vec[1], 0.8F);
        EXPECT_FLOAT_EQ(vec[2], 0.F);
    }

    TEST_F(VectorTest, NormalizeL2AlreadyNormalized)
    {
        TestVector vec{ 1.F };

        vec.normalizeL2();

        const float normalizedNorm{ vec.computeNorm() };
        EXPECT_FLOAT_EQ(normalizedNorm, 1.F);

        // Each element should be 1/sqrt(3)
        const float expectedValue{ 1.F / std::sqrt(static_cast<float>(VectorSize)) };
        for (std::size_t i{}; i < VectorSize; ++i)
            EXPECT_FLOAT_EQ(vec[i], expectedValue);
    }

    TEST_F(VectorTest, NormalizeL2NegativeValues)
    {
        TestVector vec;
        vec[0] = -3.F;
        vec[1] = -4.F;
        vec[2] = 0.F;

        vec.normalizeL2();

        const float norm{ vec.computeNorm() };
        EXPECT_FLOAT_EQ(norm, 1.F);

        EXPECT_FLOAT_EQ(vec[0], -0.6f);
        EXPECT_FLOAT_EQ(vec[1], -0.8f);
        EXPECT_FLOAT_EQ(vec[2], 0.F);
    }

    // Iterator support
    TEST_F(VectorTest, BeginEnd)
    {
        TestVector vec;
        vec[0] = 1.F;
        vec[1] = 2.F;
        vec[2] = 3.F;

        std::size_t count{};
        for (auto it{ vec.begin() }; it != vec.end(); ++it)
            ++count;

        EXPECT_EQ(count, VectorSize);
    }

    TEST_F(VectorTest, ConstBeginEnd)
    {
        const TestVector vec{ 5.F };

        std::size_t count{};
        for (auto it{ vec.cbegin() }; it != vec.cend(); ++it)
        {
            EXPECT_FLOAT_EQ(*it, 5.F);
            ++count;
        }

        EXPECT_EQ(count, VectorSize);
    }

    TEST_F(VectorTest, RangeBasedForLoop)
    {
        TestVector vec;
        vec[0] = 10.F;
        vec[1] = 20.F;
        vec[2] = 30.F;

        std::size_t index{};
        for (auto val : vec)
        {
            EXPECT_FLOAT_EQ(val, (index + 1) * 10.F);
            ++index;
        }
    }

    TEST_F(VectorTest, OperatorMinus)
    {
        TestVector vec1;
        vec1[0] = 5.F;
        vec1[1] = 6.F;
        vec1[2] = 7.F;

        TestVector vec2;
        vec2[0] = 1.F;
        vec2[1] = 2.F;
        vec2[2] = 3.F;

        TestVector result{ vec1 - vec2 };

        EXPECT_FLOAT_EQ(result[0], 4.F);
        EXPECT_FLOAT_EQ(result[1], 4.F);
        EXPECT_FLOAT_EQ(result[2], 4.F);
    }

    TEST_F(VectorTest, DoubleDataType)
    {
        Vector<3, double> doubleVec{ 1.5 };
        EXPECT_EQ(doubleVec.getSize(), 3);
        EXPECT_DOUBLE_EQ(doubleVec[0], 1.5);
    }
} // namespace lms::som