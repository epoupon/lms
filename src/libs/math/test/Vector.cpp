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

#include "math/Vector.hpp"

namespace lms::math::vectorTests
{
    constexpr float epsilon{ 1e-5F };

    TEST(Vector, constructionDefault)
    {
        Vector<3, float> v;

        EXPECT_FLOAT_EQ(v[0], 0.F);
        EXPECT_FLOAT_EQ(v[1], 0.F);
        EXPECT_FLOAT_EQ(v[2], 0.F);
    }

    TEST(Vector, constructionWithInitValue)
    {
        Vector<3, float> v{ 5.0F };

        EXPECT_FLOAT_EQ(v[0], 5.0F);
        EXPECT_FLOAT_EQ(v[1], 5.0F);
        EXPECT_FLOAT_EQ(v[2], 5.0F);
    }

    TEST(Vector, constructionWithArgs)
    {
        Vector<3, float> v{ 1.0F, 2.0F, 3.0F };

        EXPECT_FLOAT_EQ(v[0], 1.0F);
        EXPECT_FLOAT_EQ(v[1], 2.0F);
        EXPECT_FLOAT_EQ(v[2], 3.0F);
    }

    TEST(Vector, size)
    {
        Vector<3, float> v;
        EXPECT_EQ(v.getSize(), 3U);
    }

    TEST(Vector, dataAccess)
    {
        Vector<3, float> v{ 1.0F, 2.0F, 3.0F };

        const float* data = v.data();
        EXPECT_FLOAT_EQ(data[0], 1.0F);
        EXPECT_FLOAT_EQ(data[1], 2.0F);
        EXPECT_FLOAT_EQ(data[2], 3.0F);
    }

    TEST(Vector, operatorAddAssign)
    {
        Vector<3, float> a{ 1.0F, 2.0F, 3.0F };
        Vector<3, float> b{ 4.0F, 5.0F, 6.0F };

        a += b;

        EXPECT_FLOAT_EQ(a[0], 5.0F);
        EXPECT_FLOAT_EQ(a[1], 7.0F);
        EXPECT_FLOAT_EQ(a[2], 9.0F);
    }

    TEST(Vector, operatorSubAssign)
    {
        Vector<3, float> a{ 4.0F, 5.0F, 6.0F };
        Vector<3, float> b{ 1.0F, 2.0F, 3.0F };

        a -= b;

        EXPECT_FLOAT_EQ(a[0], 3.0F);
        EXPECT_FLOAT_EQ(a[1], 3.0F);
        EXPECT_FLOAT_EQ(a[2], 3.0F);
    }

    TEST(Vector, operatorMulAssign)
    {
        Vector<3, float> v{ 1.0F, 2.0F, 3.0F };

        v *= 2.0F;

        EXPECT_FLOAT_EQ(v[0], 2.0F);
        EXPECT_FLOAT_EQ(v[1], 4.0F);
        EXPECT_FLOAT_EQ(v[2], 6.0F);
    }

    TEST(Vector, operatorAdd)
    {
        Vector<3, float> a{ 1.0F, 2.0F, 3.0F };
        Vector<3, float> b{ 4.0F, 5.0F, 6.0F };

        Vector<3, float> result = a + b;

        EXPECT_FLOAT_EQ(result[0], 5.0F);
        EXPECT_FLOAT_EQ(result[1], 7.0F);
        EXPECT_FLOAT_EQ(result[2], 9.0F);

        // Ensure originals unchanged
        EXPECT_FLOAT_EQ(a[0], 1.0F);
        EXPECT_FLOAT_EQ(b[0], 4.0F);
    }

    TEST(Vector, operatorSub)
    {
        Vector<3, float> a{ 4.0F, 5.0F, 6.0F };
        Vector<3, float> b{ 1.0F, 2.0F, 3.0F };

        Vector<3, float> result = a - b;

        EXPECT_FLOAT_EQ(result[0], 3.0F);
        EXPECT_FLOAT_EQ(result[1], 3.0F);
        EXPECT_FLOAT_EQ(result[2], 3.0F);

        // Ensure originals unchanged
        EXPECT_FLOAT_EQ(a[0], 4.0F);
    }

    TEST(Vector, operatorMulScalarRight)
    {
        Vector<3, float> v{ 1.0F, 2.0F, 3.0F };

        Vector<3, float> result = v * 2.0F;

        EXPECT_FLOAT_EQ(result[0], 2.0F);
        EXPECT_FLOAT_EQ(result[1], 4.0F);
        EXPECT_FLOAT_EQ(result[2], 6.0F);

        // Ensure original unchanged
        EXPECT_FLOAT_EQ(v[0], 1.0F);
    }

    TEST(Vector, operatorMulScalarLeft)
    {
        Vector<3, float> v{ 1.0F, 2.0F, 3.0F };

        Vector<3, float> result = 3.0F * v;

        EXPECT_FLOAT_EQ(result[0], 3.0F);
        EXPECT_FLOAT_EQ(result[1], 6.0F);
        EXPECT_FLOAT_EQ(result[2], 9.0F);
    }

    TEST(Vector, computeNorm)
    {
        Vector<3, float> v{ 3.0F, 4.0F, 0.0F };

        EXPECT_FLOAT_EQ(v.computeNorm(), 5.0F);
    }

    TEST(Vector, computeNormZero)
    {
        Vector<3, float> v{ 0.0F, 0.0F, 0.0F };

        EXPECT_FLOAT_EQ(v.computeNorm(), 0.0F);
    }

    TEST(Vector, normalizeL2)
    {
        Vector<3, float> v{ 3.0F, 4.0F, 0.0F };

        v.normalizeL2();

        EXPECT_NEAR(v.computeNorm(), 1.0F, epsilon);
        EXPECT_NEAR(v[0], 0.6F, epsilon);
        EXPECT_NEAR(v[1], 0.8F, epsilon);
        EXPECT_NEAR(v[2], 0.0F, epsilon);
    }

    TEST(Vector, normalizeL2ZeroVector)
    {
        Vector<3, float> v{ 0.0F, 0.0F, 0.0F };

        v.normalizeL2();

        // Zero vector remains unchanged
        EXPECT_FLOAT_EQ(v[0], 0.0F);
        EXPECT_FLOAT_EQ(v[1], 0.0F);
        EXPECT_FLOAT_EQ(v[2], 0.0F);
    }

    TEST(Vector, normalizeL2SmallVector)
    {
        constexpr float tiny{ 1e-15F };
        Vector<3, float> v{ tiny, tiny, tiny };

        v.normalizeL2();

        // Small vector remains unchanged due to epsilon check
        EXPECT_FLOAT_EQ(v[0], tiny);
        EXPECT_FLOAT_EQ(v[1], tiny);
        EXPECT_FLOAT_EQ(v[2], tiny);
    }

    TEST(Vector, iterators)
    {
        Vector<3, float> v{ 1.0F, 2.0F, 3.0F };

        std::size_t index{};
        for (float val : v)
        {
            EXPECT_FLOAT_EQ(val, static_cast<float>(index + 1));
            ++index;
        }
    }

    TEST(Vector, constIterators)
    {
        const Vector<3, float> v{ 1.0F, 2.0F, 3.0F };

        std::size_t index{};
        for (auto it = v.cbegin(); it != v.cend(); ++it)
        {
            EXPECT_FLOAT_EQ(*it, static_cast<float>(index + 1));
            ++index;
        }
    }

    TEST(Vector, size1)
    {
        Vector<1, float> v{ 5.0F };

        EXPECT_FLOAT_EQ(v[0], 5.0F);
        EXPECT_FLOAT_EQ(v.computeNorm(), 5.0F);
    }

    TEST(Vector, largeSize)
    {
        constexpr std::size_t size{ 1000 };
        Vector<size, float> v{ 1.0F };

        EXPECT_NEAR(v.computeNorm(), std::sqrt(static_cast<float>(size)), 1e-4F);
    }

    TEST(Vector, negativeValues)
    {
        Vector<3, float> v{ -1.0F, -2.0F, -3.0F };

        EXPECT_FLOAT_EQ(v.computeNorm(), std::sqrt(14.0F));
    }

    TEST(Vector, mixedSignValues)
    {
        Vector<3, float> a{ -1.0F, 2.0F, -3.0F };
        Vector<3, float> b{ 1.0F, -2.0F, 3.0F };
        const Vector<3, float> sum{ a + b };

        EXPECT_FLOAT_EQ(sum[0], 0.0F);
        EXPECT_FLOAT_EQ(sum[1], 0.0F);
        EXPECT_FLOAT_EQ(sum[2], 0.0F);
    }

    TEST(Vector, doubleType)
    {
        Vector<3, double> v{ 1.0, 2.0, 3.0 };

        EXPECT_DOUBLE_EQ(v[0], 1.0);
        EXPECT_NEAR(v.computeNorm(), std::sqrt(14.0), 1e-15);
    }
} // namespace lms::math::vectorTests
