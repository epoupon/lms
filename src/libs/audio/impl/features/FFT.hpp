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

#include <fftw3.h>

#include "IFFT.hpp"

namespace lms::audio::features
{
    class RealFFTPlan : public IRealFFTPlan
    {
    public:
        RealFFTPlan(std::size_t n);
        ~RealFFTPlan() override;

        RealFFTPlan(const RealFFTPlan&) = delete;
        RealFFTPlan& operator=(const RealFFTPlan&) = delete;

        std::size_t getSize() const override { return _n; }

        std::span<float> getInputBuffer() override;
        std::span<std::complex<float>> getOutputBuffer() override;

        // Forward transform, from real input to complex output, not normalized
        void apply() override;

    private:
        const std::size_t _n;

        float* _input{};
        std::complex<float>* _output{};
        fftwf_plan _plan{};
    };
} // namespace lms::audio::features