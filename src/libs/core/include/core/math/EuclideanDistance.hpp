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

#include <cassert>

namespace lms::core::math
{
    template<typename T>
    T computeEuclideanSquaredDistance(const T* a, const T* b, std::size_t n)
    {
        T res{};

        for (std::size_t i{}; i < n; ++i)
        {
            const T diff{ a[i] - b[i] };
            res += diff * diff;
        }

        return res;
    }

    template<typename T>
    T computeEuclideanSquaredDistanceWithWeights(const T* a, const T* b, const T* weights, std::size_t n)
    {
        T res{};

        for (std::size_t i{}; i < n; ++i)
        {
            const T diff{ a[i] - b[i] };
            res += diff * diff * weights[i];
        }

        return res;
    }
} // namespace lms::core::math