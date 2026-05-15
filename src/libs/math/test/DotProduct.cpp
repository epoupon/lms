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

#include "math/DotProduct.hpp"

namespace lms::math::dotProductTests
{
    TEST(DotProduct, zeroLength)
    {
        const Vector<0, float> a{};
        const Vector<0, float> b{};

        EXPECT_FLOAT_EQ(computeDotProduct(a, b), 0.F);
    }

    TEST(DotProduct, simpleValues)
    {
        const Vector<3, float> a{ 1.F, 2.F, 3.F };
        const Vector<3, float> b{ 4.F, 5.F, 6.F };

        EXPECT_FLOAT_EQ(computeDotProduct(a, b), 32.F);
    }

    TEST(DotProduct, orthogonalVectors)
    {
        const Vector<3, float> a{ 1.F, 0.F, 0.F };
        const Vector<3, float> b{ 0.F, 1.F, 0.F };

        EXPECT_FLOAT_EQ(computeDotProduct(a, b), 0.F);
    }

    TEST(DotProduct, negativeValues)
    {
        const Vector<3, float> a{ -1.F, 2.F, -3.F };
        const Vector<3, float> b{ 4.F, -5.F, 6.F };

        EXPECT_FLOAT_EQ(computeDotProduct(a, b), -32.F);
    }

    TEST(DotProduct, vectorMethod)
    {
        const Vector<3, float> a{ 1.F, 2.F, 3.F };
        const Vector<3, float> b{ 4.F, 5.F, 6.F };

        EXPECT_FLOAT_EQ(computeDotProduct(a, b), 32.F);
    }

    TEST(DotProduct, functor)
    {
        const Vector<3, float> reference{ 1.F, 2.F, 3.F };
        const Vector<3, float> candidate{ 4.F, 5.F, 6.F };
        const DotProduct<3, float> dotProduct{ reference };

        EXPECT_FLOAT_EQ(dotProduct(candidate), 32.F);
    }
} // namespace lms::math::dotProductTests
