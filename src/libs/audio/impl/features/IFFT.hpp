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

#include <complex>
#include <memory>
#include <span>

namespace lms::audio::features
{
    class IRealFFTPlan
    {
    public:
        static inline constexpr std::size_t minBufferAlignment{ 32 };

        virtual ~IRealFFTPlan() = default;

        virtual std::size_t getInputSize() const = 0;  // n
        virtual std::size_t getOutputSize() const = 0; // n/2 + 1

        // Apply FFT
        // input must be size n, aligned to minBufferAlignment bytes
        // output must be size n/2 + 1, aligned to minBufferAlignment bytes
        virtual void apply(std::span<const float> input, std::span<std::complex<float>> output) = 0;
    };

    // size must be a power of 2 and >= 2 (throw if not)
    std::unique_ptr<IRealFFTPlan> createRealFFTPlan(std::size_t n);
} // namespace lms::audio::features