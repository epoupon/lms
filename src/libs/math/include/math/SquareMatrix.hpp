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

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace lms::math
{
    template<typename T, std::size_t N>
    class SquareMatrix
    {
    public:
        static_assert(N > 0, "SquareMatrix size must be positive");

        using Row = std::array<T, N>;

        constexpr SquareMatrix() = default;

        constexpr explicit SquareMatrix(const T& value)
        {
            fill(value);
        }

        constexpr void fill(const T& value)
        {
            for (auto& row : _values)
                row.fill(value);
        }

        constexpr std::size_t size() const noexcept
        {
            return N;
        }

        constexpr Row& operator[](std::size_t index)
        {
            assert(index < N);
            return _values[index];
        }

        constexpr const Row& operator[](std::size_t index) const
        {
            assert(index < N);
            return _values[index];
        }

        constexpr auto begin()
        {
            return _values.begin();
        }

        constexpr auto end()
        {
            return _values.end();
        }

        constexpr auto begin() const
        {
            return _values.begin();
        }

        constexpr auto end() const
        {
            return _values.end();
        }

        constexpr auto cbegin() const
        {
            return _values.cbegin();
        }

        constexpr auto cend() const
        {
            return _values.cend();
        }

    private:
        std::array<Row, N> _values{};
    };

    template<typename T, std::size_t N>
    bool choleskyDecompose(const SquareMatrix<T, N>& A, SquareMatrix<T, N>& L)
    {
        static_assert(std::is_floating_point_v<T>, "Cholesky decomposition requires floating point type");

        L.fill(T{});

        for (std::size_t i{}; i < N; ++i)
        {
            for (std::size_t j{}; j <= i; ++j)
            {
                T sum{};

                for (std::size_t k{}; k < j; ++k)
                    sum += L[i][k] * L[j][k];

                if (i == j)
                {
                    const T val{ A[i][i] - sum };
                    if (val <= T{})
                        return false;

                    L[i][j] = std::sqrt(val);
                }
                else
                {
                    L[i][j] = (A[i][j] - sum) / L[j][j];
                }
            }
        }

        return true;
    }

    template<typename T, std::size_t N>
    void invertLowerTriangular(const SquareMatrix<T, N>& L, SquareMatrix<T, N>& Linv)
    {
        static_assert(std::is_floating_point_v<T>, "Requires floating point type");

        Linv.fill(T{});

        for (std::size_t i{}; i < N; ++i)
        {
            assert(std::abs(L[i][i]) > std::numeric_limits<T>::epsilon());
            Linv[i][i] = T{ 1 } / L[i][i];

            for (std::size_t j{}; j < i; ++j)
            {
                T sum{};

                for (std::size_t k{ j }; k < i; ++k)
                    sum += L[i][k] * Linv[k][j];

                Linv[i][j] = -sum / L[i][i];
            }
        }
    }

    template<typename T, std::size_t N>
    T computeSymmetryMaxDiff(const SquareMatrix<T, N>& M)
    {
        static_assert(std::is_floating_point_v<T>, "Requires floating point type");

        T maxDiff{};

        for (std::size_t i{}; i < N; ++i)
        {
            for (std::size_t j{ i + 1 }; j < N; ++j)
            {
                const T diff{ std::abs(M[i][j] - M[j][i]) };
                maxDiff = std::max(maxDiff, diff);
            }
        }

        return maxDiff;
    }
} // namespace lms::math
