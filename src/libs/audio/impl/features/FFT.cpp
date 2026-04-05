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

#include "FFT.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <mutex>

#include "audio/Exception.hpp"

namespace lms::audio::features
{
    namespace
    {
        constexpr bool isPowerOfTwo(std::size_t n)
        {
            return n && !(n & (n - 1));
        }

        std::size_t validateFFTSize(std::size_t n)
        {
            if (n < 2)
                throw Exception{ "FFT size must be greater than or equal to 2" };

            if (!isPowerOfTwo(n))
                throw Exception{ "FFT size must be a power of 2" };

            return n;
        }
    } // namespace

    std::unique_ptr<IRealFFTPlan> createRealFFTPlan(std::size_t n)
    {
        return std::make_unique<RealFFTPlan>(n);
    }

    RealFFTPlan::RealFFTPlan(std::size_t n)
        : _n{ validateFFTSize(n) }
    {
        float* input{ static_cast<float*>(::fftwf_malloc(sizeof(float) * _n)) };
        if (!input)
            throw Exception{ "Cannot allocate input buffer for FFT" };

        fftwf_complex* output{ static_cast<fftwf_complex*>(::fftwf_malloc(sizeof(fftwf_complex) * (_n / 2 + 1))) };
        if (!output)
        {
            fftwf_free(input);
            throw Exception{ "Cannot allocate output buffer for FFT" };
        }

        std::fill(input, input + _n, 0.F);

        {
            // Unfortunately, fftwf_plan funcs are not thread safe
            static std::mutex mutex;
            std::scoped_lock lock{ mutex };

            _plan = ::fftwf_plan_dft_r2c_1d(
                static_cast<int>(_n), // size of FFT
                input,                // input
                output,
                FFTW_MEASURE);
        }

        if (!_plan)
            throw Exception("Failed to create FFTW plan");
    }

    RealFFTPlan::~RealFFTPlan()
    {
        ::fftwf_destroy_plan(_plan);
    }

    std::size_t RealFFTPlan::getInputSize() const
    {
        return _n;
    }

    std::size_t RealFFTPlan::getOutputSize() const
    {
        return _n / 2 + 1;
    }

    void RealFFTPlan::apply(std::span<const float> input, std::span<std::complex<float>> output)
    {
        assert(reinterpret_cast<std::uintptr_t>(input.data()) % minBufferAlignment == 0);
        assert(reinterpret_cast<std::uintptr_t>(output.data()) % minBufferAlignment == 0);
        ::fftwf_execute_dft_r2c(_plan, const_cast<float*>(input.data()), reinterpret_cast<::fftwf_complex*>(output.data()));
    }
} // namespace lms::audio::features