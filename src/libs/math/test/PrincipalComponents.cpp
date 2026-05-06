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

#include "math/PrincipalComponents.hpp"

namespace lms::math::principalComponentsTests
{
    constexpr float epsilon{ 1e-4F };
    constexpr float doubleEpsilon{ 1e-8 };

    TEST(PrincipalComponents, dotProductZeroVectors)
    {
        Vector<3, float> a{ 0.0F, 0.0F, 0.0F };
        Vector<3, float> b{ 1.0F, 2.0F, 3.0F };

        EXPECT_FLOAT_EQ(dotProduct(a, b), 0.0F);
    }

    TEST(PrincipalComponents, dotProductOrthogonal)
    {
        Vector<3, float> a{ 1.0F, 0.0F, 0.0F };
        Vector<3, float> b{ 0.0F, 1.0F, 0.0F };

        EXPECT_FLOAT_EQ(dotProduct(a, b), 0.0F);
    }

    TEST(PrincipalComponents, dotProductParallel)
    {
        Vector<3, float> a{ 1.0F, 2.0F, 3.0F };
        Vector<3, float> b{ 2.0F, 4.0F, 6.0F };

        EXPECT_FLOAT_EQ(dotProduct(a, b), 28.0F); // 2 + 8 + 18
    }

    TEST(PrincipalComponents, dotProductAntiparallel)
    {
        Vector<3, float> a{ 1.0F, 2.0F, 3.0F };
        Vector<3, float> b{ -1.0F, -2.0F, -3.0F };

        EXPECT_FLOAT_EQ(dotProduct(a, b), -14.0F);
    }

    TEST(PrincipalComponents, dotProductDouble)
    {
        Vector<3, double> a{ 0.5, 0.5, 0.5 };
        Vector<3, double> b{ 2.0, 2.0, 2.0 };

        EXPECT_DOUBLE_EQ(dotProduct(a, b), 3.0);
    }

    TEST(PrincipalComponents, pearsonCorrelationIdentical)
    {
        Vector<5, float> a{ 1.0F, 2.0F, 3.0F, 4.0F, 5.0F };
        Vector<5, float> b{ 1.0F, 2.0F, 3.0F, 4.0F, 5.0F };

        EXPECT_NEAR(pearsonCorrelation(a, b), 1.0F, epsilon);
    }

    TEST(PrincipalComponents, pearsonCorrelationNegative)
    {
        Vector<5, float> a{ 1.0F, 2.0F, 3.0F, 4.0F, 5.0F };
        Vector<5, float> b{ 5.0F, 4.0F, 3.0F, 2.0F, 1.0F };

        EXPECT_NEAR(pearsonCorrelation(a, b), -1.0F, epsilon);
    }

    TEST(PrincipalComponents, pearsonCorrelationIndependent)
    {
        Vector<4, float> a{ 1.0F, 2.0F, 3.0F, 4.0F };
        Vector<4, float> b{ 4.0F, 3.0F, 2.0F, 1.0F };

        EXPECT_NEAR(std::abs(pearsonCorrelation(a, b)), 1.0F, epsilon);
    }

    TEST(PrincipalComponents, pearsonCorrelationConstantVector)
    {
        Vector<5, float> a{ 1.0F, 2.0F, 3.0F, 4.0F, 5.0F };
        Vector<5, float> b{ 2.0F, 2.0F, 2.0F, 2.0F, 2.0F };

        // Constant vector has zero variance
        EXPECT_FLOAT_EQ(pearsonCorrelation(a, b), 0.0F);
    }

    TEST(PrincipalComponents, pearsonCorrelationBothConstant)
    {
        Vector<5, float> a{ 1.0F, 1.0F, 1.0F, 1.0F, 1.0F };
        Vector<5, float> b{ 2.0F, 2.0F, 2.0F, 2.0F, 2.0F };

        EXPECT_FLOAT_EQ(pearsonCorrelation(a, b), 0.0F);
    }

    TEST(PrincipalComponents, pearsonCorrelationWeakPositive)
    {
        Vector<4, float> a{ 1.0F, 2.0F, 3.0F, 4.0F };
        Vector<4, float> b{ 1.1F, 2.1F, 2.9F, 3.9F };

        float corr = pearsonCorrelation(a, b);
        EXPECT_GT(corr, 0.9F);
        EXPECT_LE(corr, 1.0F);
    }

    TEST(PrincipalComponents, powerIterationReturnsEigenpairs)
    {
        SquareMatrix<double, 2> covariance;
        covariance[0][0] = 2.0;
        covariance[0][1] = 0.0;
        covariance[1][0] = 0.0;
        covariance[1][1] = 1.0;

        Vector<2, double> eigenvalues{};
        std::array<Vector<2, double>, 2> eigenvectors;

        computeEigenpairsViaPowerIteration(covariance, eigenvectors, eigenvalues);

        EXPECT_NEAR(eigenvalues[0], 2.0, epsilon);
        EXPECT_NEAR(eigenvalues[1], 1.0, epsilon);

        for (std::size_t k{}; k < 2; ++k)
        {
            Vector<2, double> Av{};
            for (std::size_t i{}; i < 2; ++i)
            {
                for (std::size_t j{}; j < 2; ++j)
                    Av[i] += covariance[i][j] * eigenvectors[k][j];
            }

            EXPECT_NEAR(Av[0], eigenvalues[k] * eigenvectors[k][0], epsilon);
            EXPECT_NEAR(Av[1], eigenvalues[k] * eigenvectors[k][1], epsilon);
        }
    }

    TEST(PrincipalComponents, powerIterationIdentity)
    {
        SquareMatrix<double, 3> covariance;
        covariance.fill(0.0);
        covariance[0][0] = 1.0;
        covariance[1][1] = 1.0;
        covariance[2][2] = 1.0;

        Vector<3, double> eigenvalues{};
        std::array<Vector<3, double>, 3> eigenvectors;

        computeEigenpairsViaPowerIteration(covariance, eigenvectors, eigenvalues);

        // All eigenvalues should be 1
        EXPECT_NEAR(eigenvalues[0], 1.0, doubleEpsilon);
        EXPECT_NEAR(eigenvalues[1], 1.0, doubleEpsilon);
        EXPECT_NEAR(eigenvalues[2], 1.0, doubleEpsilon);
    }

    TEST(PrincipalComponents, powerIterationSymmetric)
    {
        SquareMatrix<double, 2> covariance;
        covariance[0][0] = 4.0;
        covariance[0][1] = 2.0;
        covariance[1][0] = 2.0;
        covariance[1][1] = 3.0;

        Vector<2, double> eigenvalues{};
        std::array<Vector<2, double>, 2> eigenvectors;

        computeEigenpairsViaPowerIteration(covariance, eigenvectors, eigenvalues);

        // Verify A*v = lambda*v for each eigenpair
        for (std::size_t k{}; k < 2; ++k)
        {
            Vector<2, double> Av{};
            for (std::size_t i{}; i < 2; ++i)
            {
                for (std::size_t j{}; j < 2; ++j)
                    Av[i] += covariance[i][j] * eigenvectors[k][j];
            }

            EXPECT_NEAR(Av[0], eigenvalues[k] * eigenvectors[k][0], epsilon);
            EXPECT_NEAR(Av[1], eigenvalues[k] * eigenvectors[k][1], epsilon);
        }
    }

    TEST(PrincipalComponents, powerIterationWithCustomIterations)
    {
        SquareMatrix<double, 2> covariance;
        covariance[0][0] = 2.0;
        covariance[0][1] = 0.0;
        covariance[1][0] = 0.0;
        covariance[1][1] = 1.0;

        Vector<2, double> eigenvalues{};
        std::array<Vector<2, double>, 2> eigenvectors;

        // Use only 50 iterations
        computeEigenpairsViaPowerIteration(covariance, eigenvectors, eigenvalues, 50, 1e-10);

        EXPECT_NEAR(eigenvalues[0], 2.0, 1e-2);
        EXPECT_NEAR(eigenvalues[1], 1.0, 1e-2);
    }

    TEST(PrincipalComponents, powerIterationWithCustomEpsilon)
    {
        SquareMatrix<double, 2> covariance;
        covariance[0][0] = 2.0;
        covariance[0][1] = 0.0;
        covariance[1][0] = 0.0;
        covariance[1][1] = 1.0;

        Vector<2, double> eigenvalues{};
        std::array<Vector<2, double>, 2> eigenvectors;

        // Use looser epsilon
        computeEigenpairsViaPowerIteration(covariance, eigenvectors, eigenvalues, 200, 1e-6);

        EXPECT_NEAR(eigenvalues[0], 2.0, epsilon);
        EXPECT_NEAR(eigenvalues[1], 1.0, epsilon);
    }

    TEST(PrincipalComponents, powerIterationLargerMatrix)
    {
        // Create a 4x4 diagonal matrix
        SquareMatrix<double, 4> covariance;
        covariance.fill(0.0);
        covariance[0][0] = 4.0;
        covariance[1][1] = 3.0;
        covariance[2][2] = 2.0;
        covariance[3][3] = 1.0;

        Vector<4, double> eigenvalues{};
        std::array<Vector<4, double>, 4> eigenvectors;

        computeEigenpairsViaPowerIteration(covariance, eigenvectors, eigenvalues);

        // Eigenvalues should be 4, 3, 2, 1 (in descending order after deflation)
        EXPECT_NEAR(eigenvalues[0], 4.0, doubleEpsilon);
        EXPECT_NEAR(eigenvalues[1], 3.0, doubleEpsilon);
        EXPECT_NEAR(eigenvalues[2], 2.0, doubleEpsilon);
        EXPECT_NEAR(eigenvalues[3], 1.0, doubleEpsilon);
    }

    TEST(PrincipalComponents, projectOntoBasis)
    {
        std::array<std::array<float, 2>, 2> basis{};
        basis[0][0] = 1.0F;
        basis[0][1] = 0.0F;
        basis[1][0] = 0.0F;
        basis[1][1] = 1.0F;

        Vector<2, float> centered{ 1.0F, 2.0F };
        Vector<2, float> output;
        std::array<float, 2> scales{ 2.0F, 3.0F };

        projectOntoBasis(basis, centered, output, scales);

        EXPECT_FLOAT_EQ(output[0], 2.0F);
        EXPECT_FLOAT_EQ(output[1], 6.0F);
    }

    TEST(PrincipalComponents, pearsonCorrelation)
    {
        Vector<3, float> a{ 1.0F, 2.0F, 3.0F };
        Vector<3, float> b{ 1.0F, 2.0F, 3.0F };
        Vector<3, float> c{ -1.0F, -2.0F, -3.0F };

        EXPECT_NEAR(pearsonCorrelation(a, b), 1.0F, epsilon);
        EXPECT_NEAR(pearsonCorrelation(a, c), -1.0F, epsilon);
    }
} // namespace lms::math::principalComponentsTests
