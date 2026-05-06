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

#include "math/CentroidCalculator.hpp"
#include "math/Vector.hpp"

namespace lms::math::centroidCalculatorTests
{
    TEST(CentroidCalculator, initialState)
    {
        CentroidCalculator<Vector<3, float>> calculator;

        EXPECT_TRUE(calculator.empty());
        EXPECT_EQ(calculator.count(), 0U);
    }

    TEST(CentroidCalculator, addAndFinalize)
    {
        CentroidCalculator<Vector<3, float>> calculator;
        calculator.add(Vector<3, float>{ 1.0F, 2.0F, 3.0F });
        calculator.add(Vector<3, float>{ 4.0F, 5.0F, 6.0F });

        const Vector<3, float> result = calculator.finalize();

        EXPECT_FLOAT_EQ(result[0], 2.5F);
        EXPECT_FLOAT_EQ(result[1], 3.5F);
        EXPECT_FLOAT_EQ(result[2], 4.5F);
    }

    TEST(CentroidCalculator, finalizeNormalized)
    {
        CentroidCalculator<Vector<2, float>> calculator;
        calculator.add(Vector<2, float>{ 3.0F, 4.0F });

        const Vector<2, float> result = calculator.finalizeNormalized();

        EXPECT_NEAR(result.computeNorm(), 1.0F, 1e-6F);
        EXPECT_NEAR(result[0], 0.6F, 1e-6F);
        EXPECT_NEAR(result[1], 0.8F, 1e-6F);
    }

    TEST(CentroidCalculator, computeCentroidSpan)
    {
        const std::array<Vector<2, float>, 2> values{
            Vector<2, float>{ 0.0F, 2.0F },
            Vector<2, float>{ 2.0F, 0.0F }
        };

        const Vector<2, float> result = computeCentroid(std::span<const Vector<2, float>>(values));

        EXPECT_FLOAT_EQ(result[0], 1.0F);
        EXPECT_FLOAT_EQ(result[1], 1.0F);
    }
} // namespace lms::math::centroidCalculatorTests
