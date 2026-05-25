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

#include <type_traits>

#include "math/SquareMatrix.hpp"
#include "math/Vector.hpp"

namespace lms::math
{
    template<std::size_t Size, typename FloatType = float>
    class CovarianceMatrixCalculator
    {
    public:
        static_assert(!std::is_const_v<FloatType>);

        using CovarianceMatrix = SquareMatrix<FloatType, Size>;

        constexpr void add(const Vector<Size, FloatType>& centeredVector)
        {
            for (std::size_t i{}; i < Size; ++i)
            {
                for (std::size_t j{}; j <= i; ++j)
                    _cov[i][j] += centeredVector[i] * centeredVector[j];
            }

            ++_count;
        }

        template<typename InputIt>
        constexpr void add(InputIt first, InputIt last)
        {
            for (; first != last; ++first)
                add(*first);
        }

        constexpr void finalizeSample(CovarianceMatrix& out) const
        {
            out.fill(FloatType{});
            if (_count < 2)
                return;

            const FloatType divisor{ static_cast<FloatType>(_count - 1) };
            for (std::size_t i{}; i < Size; ++i)
            {
                for (std::size_t j{}; j <= i; ++j)
                {
                    const FloatType value{ _cov[i][j] / divisor };
                    out[i][j] = value;
                    out[j][i] = value;
                }
            }
        }

        constexpr void finalizePopulation(CovarianceMatrix& out) const
        {
            out.fill(FloatType{});
            if (_count == 0)
                return;

            const FloatType divisor{ static_cast<FloatType>(_count) };
            for (std::size_t i{}; i < Size; ++i)
            {
                for (std::size_t j{}; j <= i; ++j)
                {
                    const FloatType value{ _cov[i][j] / divisor };
                    out[i][j] = value;
                    out[j][i] = value;
                }
            }
        }

        constexpr bool empty() const
        {
            return _count == 0;
        }

        constexpr std::size_t count() const
        {
            return _count;
        }

    private:
        CovarianceMatrix _cov{};
        std::size_t _count{};
    };
} // namespace lms::math
