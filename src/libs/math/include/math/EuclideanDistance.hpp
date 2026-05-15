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

#include "math/Vector.hpp"

namespace lms::math
{
    template<std::size_t Size, typename FloatType>
    constexpr FloatType computeEuclideanSquaredDistance(const Vector<Size, FloatType>& a, const Vector<Size, FloatType>& b)
    {
        FloatType res{};

        for (std::size_t i{}; i < Size; ++i)
        {
            const FloatType diff{ a[i] - b[i] };
            res += diff * diff;
        }

        return res;
    }

    template<std::size_t Size, typename FloatType>
    constexpr FloatType computeEuclideanSquaredDistanceWithWeights(const Vector<Size, FloatType>& a, const Vector<Size, FloatType>& b, const Vector<Size, FloatType>& weights)
    {
        FloatType res{};

        for (std::size_t i{}; i < Size; ++i)
        {
            const FloatType diff{ a[i] - b[i] };
            res += diff * diff * weights[i];
        }

        return res;
    }

    template<std::size_t Size, typename FloatType = float>
    struct SquaredEuclideanDistance
    {
        constexpr SquaredEuclideanDistance(const Vector<Size, FloatType>& ref)
            : _ref{ ref } {}

        constexpr FloatType operator()(const Vector<Size, FloatType>& a) const
        {
            return computeEuclideanSquaredDistance(_ref, a);
        }

        const Vector<Size, FloatType>& _ref;
    };

    template<std::size_t Size, typename FloatType = float>
    struct SquaredEuclideanDistanceWithWeights
    {
        constexpr SquaredEuclideanDistanceWithWeights(const Vector<Size, FloatType>& ref, const Vector<Size, FloatType>& weights)
            : _ref{ ref }
            , _weights{ weights }
        {
        }

        constexpr FloatType operator()(const Vector<Size, FloatType>& a) const
        {
            return computeEuclideanSquaredDistanceWithWeights(_ref, a, _weights);
        }

        const Vector<Size, FloatType>& _ref;
        const Vector<Size, FloatType>& _weights;
    };
} // namespace lms::math