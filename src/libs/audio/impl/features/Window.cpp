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

#include "Window.hpp"

#include <cmath>

#include "audio/Exception.hpp"

namespace lms::audio::features
{
    void computeHannWindow(std::span<float> window)
    {
        const std::size_t frameSize{ window.size() };
        if (frameSize == 0)
            throw Exception{ "Invalid frame size" };

        for (size_t i{}; i < frameSize; ++i)
            window[i] = 0.5F * (1.0F - std::cos(2.F * M_PI * i / (static_cast<double>(frameSize) - 1)));
    }
} // namespace lms::audio::features