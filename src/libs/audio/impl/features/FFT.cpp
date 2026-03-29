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
        _input = static_cast<float*>(::fftwf_malloc(sizeof(float) * _n));
        if (!_input)
            throw Exception{ "Cannot allocate input buffer for FFT" };

        _output = static_cast<std::complex<float>*>(::fftwf_malloc(sizeof(fftwf_complex) * (_n / 2 + 1)));
        if (!_output)
        {
            fftwf_free(_input);
            throw Exception{ "Cannot allocate output buffer for FFT" };
        }

        std::fill(_input, _input + _n, 0.F);

        _plan = ::fftwf_plan_dft_r2c_1d(
            static_cast<int>(_n), // size of FFT
            _input,               // input
            reinterpret_cast<fftwf_complex*>(_output),
            FFTW_MEASURE);

        if (!_plan)
            throw std::runtime_error("Failed to create FFTW plan");
    }

    RealFFTPlan::~RealFFTPlan()
    {
        ::fftwf_destroy_plan(_plan);
        ::fftwf_free(_input);
        ::fftwf_free(_output);
    }

    std::span<float> RealFFTPlan::getInputBuffer()
    {
        return std::span<float>{ _input, _n };
    }

    std::span<std::complex<float>> RealFFTPlan::getOutputBuffer()
    {
        return std::span<std::complex<float>>{ _output, _n / 2 + 1 };
    }

    void RealFFTPlan::apply()
    {
        ::fftwf_execute_dft_r2c(_plan, _input, reinterpret_cast<fftwf_complex*>(_output));
    }
} // namespace lms::audio::features