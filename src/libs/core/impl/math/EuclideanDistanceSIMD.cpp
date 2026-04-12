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

static_assert(LMS_SUPPORT_AVX2);

#include <immintrin.h>

namespace lms::core::math::detail
{
    float computeEuclideanSquaredDistanceSIMD(const float* a, const float* b, std::size_t n)
    {
        __m256 acc{ _mm256_setzero_ps() };

        std::size_t i{};
        for (; i + 8 <= n; i += 8)
        {
            __m256 va{ _mm256_loadu_ps(a + i) };
            __m256 vb{ _mm256_loadu_ps(b + i) };

            __m256 diff{ _mm256_sub_ps(va, vb) };
            __m256 sq{ _mm256_mul_ps(diff, diff) };

            acc = _mm256_add_ps(acc, sq);
        }

        __m128 sum{ _mm_add_ps(
            _mm256_castps256_ps128(acc),
            _mm256_extractf128_ps(acc, 1)) };

        __m128 hi{ _mm_movehl_ps(sum, sum) };
        sum = _mm_add_ps(sum, hi);
        sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 1));

        float result{ _mm_cvtss_f32(sum) };

        for (; i < n; ++i)
        {
            const float diff{ a[i] - b[i] };
            result += diff * diff;
        }

        return result;
    }

    float computeEuclideanSquaredDistanceSIMD(const float* a, const float* b, const float* weights, std::size_t n)
    {
        __m256 acc{ _mm256_setzero_ps() };

        std::size_t i{};
        for (; i + 8 <= n; i += 8)
        {
            __m256 va{ _mm256_loadu_ps(a + i) };
            __m256 vb{ _mm256_loadu_ps(b + i) };
            __m256 vw{ _mm256_loadu_ps(weights + i) };

            __m256 diff{ _mm256_sub_ps(va, vb) };
            __m256 sq{ _mm256_mul_ps(diff, diff) };
            __m256 mul{ _mm256_mul_ps(sq, vw) };

            acc = _mm256_add_ps(acc, mul);
        }

        __m128 sum{ _mm_add_ps(
            _mm256_castps256_ps128(acc),
            _mm256_extractf128_ps(acc, 1)) };

        __m128 hi{ _mm_movehl_ps(sum, sum) };
        sum = _mm_add_ps(sum, hi);
        sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 1));

        float result{ _mm_cvtss_f32(sum) };

        for (; i < n; ++i)
        {
            const float diff{ a[i] - b[i] };
            result += diff * diff * weights[i];
        }

        return result;
    }
} // namespace lms::core::math::detail