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

#include "core/math/EuclideanDistance.hpp"

namespace lms::core::math
{
    namespace detail
    {
        float computeEuclideanSquaredDistance(const float* a, const float* b, std::size_t n)
        {
            float res{};

            for (std::size_t i{}; i < n; ++i)
            {
                const float diff{ a[i] - b[i] };
                res += diff * diff;
            }

            return res;
        }

        float computeEuclideanSquaredDistance(const float* a, const float* b, const float* weights, std::size_t n)
        {
            float res{};

            for (std::size_t i{}; i < n; ++i)
            {
                const float diff{ a[i] - b[i] };
                res += diff * diff * weights[i];
            }

            return res;
        }

#if LMS_SUPPORT_AVX2
        bool hasAvx2Support()
        {
            return __builtin_cpu_supports("avx2");
        }

        static const bool avx2Available{ hasAvx2Support() };

        float computeEuclideanSquaredDistanceSIMD(const float* a, const float* b, std::size_t n);
        float computeEuclideanSquaredDistanceSIMD(const float* a, const float* b, const float* weights, std::size_t n);
#endif // LMS_SUPPORT_AVX2
    } // namespace detail

    float computeEuclideanSquaredDistance(const float* a, const float* b, std::size_t n)
    {
#if LMS_SUPPORT_AVX2
        if (detail::avx2Available)
            return detail::computeEuclideanSquaredDistanceSIMD(a, b, n);
#endif // LMS_SUPPORT_AVX2

        return detail::computeEuclideanSquaredDistance(a, b, n);
    }

    float computeEuclideanSquaredDistance(const float* a, const float* b, const float* weights, std::size_t n)
    {
#if LMS_SUPPORT_AVX2
        if (detail::avx2Available)
            return detail::computeEuclideanSquaredDistanceSIMD(a, b, weights, n);
#endif // LMS_SUPPORT_AVX2

        return detail::computeEuclideanSquaredDistance(a, b, weights, n);
    }

} // namespace lms::core::math