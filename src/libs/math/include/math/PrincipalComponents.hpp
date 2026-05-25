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

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <random>

#include "math/SquareMatrix.hpp"
#include "math/Vector.hpp"

namespace lms::math
{
    template<std::size_t Size, typename FloatType>
    FloatType dotProduct(const Vector<Size, FloatType>& a, const Vector<Size, FloatType>& b)
    {
        FloatType result{};
        for (std::size_t i{}; i < Size; ++i)
            result += a[i] * b[i];
        return result;
    }

    template<typename FloatType, std::size_t Size>
    void computeEigenpairsViaPowerIteration(const SquareMatrix<FloatType, Size>& covariance,
                                            std::array<Vector<Size, FloatType>, Size>& eigenvectors,
                                            Vector<Size, FloatType>& eigenvalues,
                                            std::size_t maxIterations = 200,
                                            FloatType epsilon = static_cast<FloatType>(1e-15))
    {
        // Power iteration with Deflation for computing eigendecomposition.
        // Iteratively finds the largest eigenvalue and corresponding eigenvector,
        // then removes it from the matrix and repeats.

        auto covarianceCopy{ std::make_unique<SquareMatrix<FloatType, Size>>(covariance) };
        std::minstd_rand rng{ 42 };
        std::uniform_real_distribution<FloatType> dist{ static_cast<FloatType>(-1.0), static_cast<FloatType>(1.0) };

        for (std::size_t k{}; k < Size; ++k)
        {
            Vector<Size, FloatType> v;
            for (std::size_t i{}; i < Size; ++i)
                v[i] = dist(rng);

            FloatType prevEigenvalue{};
            for (std::size_t iter{}; iter < maxIterations; ++iter)
            {
                Vector<Size, FloatType> Av;
                for (std::size_t i{}; i < Size; ++i)
                {
                    for (std::size_t j{}; j < Size; ++j)
                        Av[i] += (*covarianceCopy)[i][j] * v[j];
                }

                FloatType normSquared{};
                for (std::size_t i{}; i < Size; ++i)
                    normSquared += Av[i] * Av[i];

                if (normSquared < epsilon)
                    break;

                const FloatType norm{ std::sqrt(normSquared) };
                for (std::size_t i{}; i < Size; ++i)
                    v[i] = Av[i] / norm;

                eigenvalues[k] = norm;

                // Early exit if eigenvalue converged
                if (iter > 0 && std::abs(norm - prevEigenvalue) < epsilon)
                    break;

                prevEigenvalue = norm;
            }

            eigenvectors[k] = v;

            // Deflate matrix: A = A - lambda * v * v^T
            for (std::size_t i{}; i < Size; ++i)
            {
                for (std::size_t j{}; j < Size; ++j)
                    (*covarianceCopy)[i][j] -= eigenvalues[k] * v[i] * v[j];
            }
        }
    }

    template<std::size_t BasisCount, std::size_t FeatureCount, typename FloatType>
    void projectOntoBasis(const std::array<std::array<FloatType, FeatureCount>, BasisCount>& basis,
                          const Vector<FeatureCount, FloatType>& centered,
                          Vector<BasisCount, FloatType>& output,
                          const std::array<FloatType, BasisCount>& scales)
    {
        for (std::size_t k{}; k < BasisCount; ++k)
        {
            FloatType sum{};
            for (std::size_t j{}; j < FeatureCount; ++j)
                sum += basis[k][j] * centered[j];
            output[k] = sum * scales[k];
        }
    }

    template<std::size_t Size, typename FloatType>
    FloatType pearsonCorrelation(const Vector<Size, FloatType>& a, const Vector<Size, FloatType>& b)
    {
        static_assert(Size > 0);

        FloatType meanA{};
        FloatType meanB{};
        for (std::size_t i{}; i < Size; ++i)
        {
            meanA += a[i];
            meanB += b[i];
        }

        meanA /= static_cast<FloatType>(Size);
        meanB /= static_cast<FloatType>(Size);

        FloatType cov{};
        FloatType varA{};
        FloatType varB{};
        for (std::size_t i{}; i < Size; ++i)
        {
            const FloatType da{ a[i] - meanA };
            const FloatType db{ b[i] - meanB };
            cov += da * db;
            varA += da * da;
            varB += db * db;
        }

        if (varA <= static_cast<FloatType>(0) || varB <= static_cast<FloatType>(0))
            return static_cast<FloatType>(0);

        return cov / std::sqrt(varA * varB);
    }
} // namespace lms::math
