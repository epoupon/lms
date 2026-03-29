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

#include "DeltaCalculator.hpp"

#include <cassert>

#include "audio/Exception.hpp"
namespace lms::audio::features
{
    namespace detail
    {
        std::size_t check(std::size_t windowSize)
        {
            if (windowSize % 2 != 1)
                throw Exception{ "Window size " + std::to_string(windowSize) + " is not odd for DeltaCalculator!" };

            return windowSize;
        }

        float denom(std::size_t halfWindowSize)
        {
            float s{};

            for (std::size_t i{ 1 }; i <= halfWindowSize; ++i)
                s += static_cast<float>(i * i);

            return 2.F * s;
        }
    } // namespace detail

    DeltaCalculator::DeltaCalculator(std::size_t w)
        : _windowSize{ detail::check(w) }
        , _halfWindowSize{ (w - 1) / 2 }
        , _invDenominator{ 1.f / detail::denom(_halfWindowSize) }
        , _buffer(2 * w, 0.F)
    {
    }

    std::optional<float> DeltaCalculator::add(float sample)
    {
        _buffer[_index] = sample;
        _buffer[_index + _windowSize] = sample;

        if (++_index == _windowSize)
            _index = 0;

        if (_count < _windowSize && ++_count < _windowSize)
            return std::nullopt;

        const std::size_t center{ _index + _windowSize - _halfWindowSize - 1 };
        const float* c{ &_buffer[center] };

        float acc{};
        for (std::size_t n{ 1 }; n <= _halfWindowSize; ++n)
            acc += n * (c[n] - c[-static_cast<int>(n)]);

        return acc * _invDenominator;
    }

    void DeltaCalculator::reset()
    {
        _index = 0;
        _count = 0;
    }
} // namespace lms::audio::features