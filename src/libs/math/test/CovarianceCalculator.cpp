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

#include "math/CovarianceCalculator.hpp"
#include "math/SquareMatrix.hpp"
#include "math/Vector.hpp"

namespace lms::math::covarianceCalculatorTests
{
    constexpr float epsilon{ 1e-6F };

    TEST(CovarianceCalculator, empty)
    {
        CovarianceMatrixCalculator<2, float> calculator;

        EXPECT_TRUE(calculator.empty());
        EXPECT_EQ(calculator.count(), 0U);
    }

    TEST(CovarianceCalculator, sampleCovariance)
    {
        CovarianceMatrixCalculator<2, float> calculator;
        calculator.add({ 1.0F, 0.0F });
        calculator.add({ -1.0F, 0.0F });

        SquareMatrix<float, 2> covariance;
        calculator.finalizeSample(covariance);

        EXPECT_NEAR(covariance[0][0], 2.0F, epsilon);
        EXPECT_NEAR(covariance[0][1], 0.0F, epsilon);
        EXPECT_NEAR(covariance[1][0], 0.0F, epsilon);
        EXPECT_NEAR(covariance[1][1], 0.0F, epsilon);
    }

    TEST(CovarianceCalculator, populationCovariance)
    {
        CovarianceMatrixCalculator<2, float> calculator;
        calculator.add(Vector<2, float>{ 1.0F, 0.0F });
        calculator.add(Vector<2, float>{ -1.0F, 0.0F });

        SquareMatrix<float, 2> covariance;
        calculator.finalizePopulation(covariance);

        EXPECT_NEAR(covariance[0][0], 1.0F, epsilon);
        EXPECT_NEAR(covariance[0][1], 0.0F, epsilon);
        EXPECT_NEAR(covariance[1][0], 0.0F, epsilon);
        EXPECT_NEAR(covariance[1][1], 0.0F, epsilon);
    }

    TEST(CovarianceCalculator, singleValueReturnsZero)
    {
        CovarianceMatrixCalculator<2, float> calculator;
        calculator.add({ 1.0F, 2.0F });

        SquareMatrix<float, 2> covariance;
        calculator.finalizeSample(covariance);

        EXPECT_FLOAT_EQ(covariance[0][0], 0.0F);
        EXPECT_FLOAT_EQ(covariance[0][1], 0.0F);
        EXPECT_FLOAT_EQ(covariance[1][0], 0.0F);
        EXPECT_FLOAT_EQ(covariance[1][1], 0.0F);
    }
} // namespace lms::math::covarianceCalculatorTests
