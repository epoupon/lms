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

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "math/Window.hpp"

namespace lms::math::tests
{
    TEST(Window, oneSampleWindowIsFinite)
    {
        std::vector<float> window(1);

        computeHannWindow<float>(window);

        EXPECT_TRUE(std::isfinite(window[0]));
        EXPECT_GE(window[0], 0.F);
        EXPECT_LE(window[0], 1.F);
    }

    TEST(Window, twoSamplesWindow)
    {
        std::vector<float> window(2);

        computeHannWindow<float>(window);

        EXPECT_FLOAT_EQ(window[0], 0.F);
        EXPECT_FLOAT_EQ(window[1], 0.F);
    }

    TEST(Window, coefficientsAreFiniteAndInRange)
    {
        std::vector<float> window(17);

        computeHannWindow<float>(window);

        for (float v : window)
        {
            EXPECT_TRUE(std::isfinite(v));
            EXPECT_GE(v, 0.F);
            EXPECT_LE(v, 1.F);
        }
    }

    TEST(Window, symmetric)
    {
        std::vector<float> window(31);

        computeHannWindow<float>(window);

        for (std::size_t i{}; i < window.size() / 2; ++i)
            EXPECT_NEAR(window[i], window[window.size() - 1 - i], 1e-6F);
    }
} // namespace lms::math::tests