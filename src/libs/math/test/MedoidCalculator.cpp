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

#include "math/MedoidCalculator.hpp"
#include "math/Vector.hpp"

namespace lms::math::medoidCalculatorTests
{
    TEST(MedoidCalculator, initialState)
    {
        MedoidCalculator<Vector<3, float>> calculator;

        EXPECT_TRUE(calculator.empty());
        EXPECT_EQ(calculator.count(), 0U);
    }

    TEST(MedoidCalculator, singleVector)
    {
        MedoidCalculator<Vector<3, float>> calculator;
        const Vector<3, float> vec{ 1.0F, 2.0F, 3.0F };
        calculator.add(vec);

        EXPECT_FALSE(calculator.empty());
        EXPECT_EQ(calculator.count(), 1U);
        EXPECT_EQ(calculator.findMedoidIndex(), 0U);

        const Vector<3, float> result = calculator.finalize();
        EXPECT_EQ(result[0], 1.0F);
        EXPECT_EQ(result[1], 2.0F);
        EXPECT_EQ(result[2], 3.0F);
    }

    TEST(MedoidCalculator, twoVectors)
    {
        MedoidCalculator<Vector<2, float>> calculator;
        const Vector<2, float> v1{ 0.0F, 0.0F };
        const Vector<2, float> v2{ 4.0F, 0.0F };

        calculator.add(v1);
        calculator.add(v2);

        EXPECT_EQ(calculator.count(), 2U);
        // Both have equal distance to the other, but first one is returned
        const std::size_t medoidIndex = calculator.findMedoidIndex();
        EXPECT_TRUE(medoidIndex == 0 || medoidIndex == 1);
    }

    TEST(MedoidCalculator, threeDifferentVectors)
    {
        MedoidCalculator<Vector<2, float>> calculator;
        // Three points: (0,0), (1,0), (10,0)
        // Medoid should be (1,0) as it's closest to the others
        calculator.add(Vector<2, float>{ 0.0F, 0.0F });
        calculator.add(Vector<2, float>{ 1.0F, 0.0F });
        calculator.add(Vector<2, float>{ 10.0F, 0.0F });

        const std::size_t medoidIndex = calculator.findMedoidIndex();
        EXPECT_EQ(medoidIndex, 1U); // The middle point (1,0) is the medoid

        const Vector<2, float> result = calculator.finalize();
        EXPECT_FLOAT_EQ(result[0], 1.0F);
        EXPECT_FLOAT_EQ(result[1], 0.0F);
    }

    TEST(MedoidCalculator, computeMedoidSpan)
    {
        const std::array<Vector<2, float>, 3> values{
            Vector<2, float>{ 0.0F, 0.0F },
            Vector<2, float>{ 1.0F, 0.0F },
            Vector<2, float>{ 10.0F, 0.0F }
        };

        const Vector<2, float> result = computeMedoid(std::span<const Vector<2, float>>(values));

        EXPECT_FLOAT_EQ(result[0], 1.0F);
        EXPECT_FLOAT_EQ(result[1], 0.0F);
    }

    TEST(MedoidCalculator, getVector)
    {
        MedoidCalculator<Vector<2, float>> calculator;
        calculator.add(Vector<2, float>{ 1.0F, 2.0F });
        calculator.add(Vector<2, float>{ 3.0F, 4.0F });

        const Vector<2, float>& v0 = calculator.getVector(0U);
        const Vector<2, float>& v1 = calculator.getVector(1U);

        EXPECT_FLOAT_EQ(v0[0], 1.0F);
        EXPECT_FLOAT_EQ(v0[1], 2.0F);
        EXPECT_FLOAT_EQ(v1[0], 3.0F);
        EXPECT_FLOAT_EQ(v1[1], 4.0F);
    }

    TEST(MedoidCalculator, clear)
    {
        MedoidCalculator<Vector<2, float>> calculator;
        calculator.add(Vector<2, float>{ 1.0F, 2.0F });
        EXPECT_EQ(calculator.count(), 1);
        calculator.clear();
        EXPECT_EQ(calculator.count(), 0);
    }
} // namespace lms::math::medoidCalculatorTests
