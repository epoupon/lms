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
#include <cmath>
#include <numbers>
#include <span>

namespace lms::math
{
    // Precondition: window.size() > 0.
    template<typename FloatType>
    void computeHannWindow(std::span<FloatType> window)
    {
        const std::size_t frameSize{ window.size() };
        assert(frameSize > 0);

        if (frameSize == 1)
        {
            window[0] = 1.F;
            return;
        }

        for (std::size_t i{}; i < frameSize; ++i)
            window[i] = static_cast<FloatType>(0.5) * (1.0F - std::cos(static_cast<FloatType>(2) * std::numbers::pi_v<FloatType> * static_cast<FloatType>(i) / (static_cast<FloatType>(frameSize) - 1.F)));
    }
} // namespace lms::math