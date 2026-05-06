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

#include "math/SquareMatrix.hpp"

namespace lms::math::squareMatrixTests
{
    constexpr float epsilon{ 1e-4F };

    TEST(SquareMatrix, choleskyDecomposePositiveDefinite)
    {
        SquareMatrix<float, 3> matrix;
        matrix.fill(0.F);

        matrix[0][0] = 4.F;
        matrix[0][1] = 12.F;
        matrix[0][2] = -16.F;
        matrix[1][0] = 12.F;
        matrix[1][1] = 37.F;
        matrix[1][2] = -43.F;
        matrix[2][0] = -16.F;
        matrix[2][1] = -43.F;
        matrix[2][2] = 98.F;

        SquareMatrix<float, 3> lower;
        EXPECT_TRUE(choleskyDecompose(matrix, lower));

        EXPECT_FLOAT_EQ(lower[0][0], 2.F);
        EXPECT_FLOAT_EQ(lower[1][0], 6.F);
        EXPECT_FLOAT_EQ(lower[1][1], 1.F);
        EXPECT_FLOAT_EQ(lower[2][0], -8.F);
        EXPECT_FLOAT_EQ(lower[2][1], 5.F);
        EXPECT_FLOAT_EQ(lower[2][2], 3.F);
    }

    TEST(SquareMatrix, choleskyDecomposeIdentity)
    {
        SquareMatrix<float, 3> matrix;
        matrix.fill(0.F);

        matrix[0][0] = 1.F;
        matrix[1][1] = 1.F;
        matrix[2][2] = 1.F;

        SquareMatrix<float, 3> lower;
        EXPECT_TRUE(choleskyDecompose(matrix, lower));

        for (std::size_t i{}; i < 3; ++i)
        {
            for (std::size_t j{}; j < 3; ++j)
            {
                if (i == j)
                    EXPECT_FLOAT_EQ(lower[i][j], 1.F);
                else
                    EXPECT_FLOAT_EQ(lower[i][j], 0.F);
            }
        }
    }

    TEST(SquareMatrix, choleskyDecomposeSize1)
    {
        SquareMatrix<float, 1> matrix;
        matrix[0][0] = 9.F;

        SquareMatrix<float, 1> lower;
        EXPECT_TRUE(choleskyDecompose(matrix, lower));
        EXPECT_FLOAT_EQ(lower[0][0], 3.F);
    }

    TEST(SquareMatrix, choleskyDecomposeNonPositiveDefinite)
    {
        SquareMatrix<float, 2> matrix;
        matrix.fill(0.F);

        SquareMatrix<float, 2> lower;
        EXPECT_FALSE(choleskyDecompose(matrix, lower));
    }

    TEST(SquareMatrix, invertLowerTriangular)
    {
        SquareMatrix<float, 3> lower;
        lower.fill(0.F);

        lower[0][0] = 2.F;
        lower[1][0] = 6.F;
        lower[1][1] = 1.F;
        lower[2][0] = -8.F;
        lower[2][1] = 5.F;
        lower[2][2] = 3.F;

        SquareMatrix<float, 3> inverse;
        invertLowerTriangular(lower, inverse);

        SquareMatrix<float, 3> identity;
        identity.fill(0.F);

        for (std::size_t i{}; i < 3; ++i)
        {
            for (std::size_t j{}; j < 3; ++j)
            {
                float sum = 0.F;
                for (std::size_t k{}; k < 3; ++k)
                {
                    sum += lower[i][k] * inverse[k][j];
                }
                identity[i][j] = sum;
            }
        }

        for (std::size_t i{}; i < 3; ++i)
        {
            for (std::size_t j{}; j < 3; ++j)
            {
                if (i == j)
                    EXPECT_NEAR(identity[i][j], 1.F, epsilon);
                else
                    EXPECT_NEAR(identity[i][j], 0.F, epsilon);
            }
        }
    }

    TEST(SquareMatrix, invertLowerTriangularSize1)
    {
        SquareMatrix<float, 1> lower;
        lower[0][0] = 5.F;

        SquareMatrix<float, 1> inverse;
        invertLowerTriangular(lower, inverse);

        EXPECT_FLOAT_EQ(inverse[0][0], 0.2F);
    }

    TEST(SquareMatrix, computeSymmetryMaxDiff)
    {
        SquareMatrix<float, 2> matrix;
        matrix.fill(0.F);

        matrix[0][1] = 1.F;
        matrix[1][0] = 1.2F;

        EXPECT_NEAR(computeSymmetryMaxDiff(matrix), 0.2F, epsilon);
    }
} // namespace lms::math::squareMatrixTests
