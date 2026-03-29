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

#include <optional>
#include <vector>

namespace lms::audio::features
{
    class DeltaCalculator
    {
    public:
        explicit DeltaCalculator(std::size_t windowSize);

        std::size_t getWindowSize() const { return _windowSize; }

        std::optional<float> add(float sample);
        void reset();

    private:
        std::size_t _windowSize;
        std::size_t _halfWindowSize;
        float _invDenominator;

        std::vector<float> _buffer;  // size = 2 * windowSize
        std::size_t _index{};
        std::size_t _count{};
    };
} // namespace lms::audio::features