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

#include <cmath>

#include "math/DotProduct.hpp"
#include "math/Vector.hpp"

namespace lms::math
{
    // Returns the cosine distance in [0, 1] for L2-normalized vectors:
    //   0 = identical direction, 0.5 = orthogonal, 1 = opposite directions.
    template<std::size_t Size, typename FloatType>
    constexpr FloatType computeNormalizedCosineDistance(
        const Vector<Size, FloatType>& a,
        const Vector<Size, FloatType>& b)
    {
        return (FloatType{ 1 } - computeDotProduct(a, b)) / FloatType{ 2 };
    }

    template<std::size_t Size, typename FloatType = float>
    struct NormalizedCosineDistance
    {
        constexpr NormalizedCosineDistance(const Vector<Size, FloatType>& ref)
            : _ref{ ref }
        {
        }

        constexpr FloatType operator()(const Vector<Size, FloatType>& a) const
        {
            return (FloatType{ 1 } - computeDotProduct(_ref, a)) / FloatType{ 2 };
        }

        const Vector<Size, FloatType>& _ref;
    };
} // namespace lms::math