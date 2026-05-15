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
        const HannWindow<1, float> window;
        const auto values{ window.values() };

        EXPECT_TRUE(std::isfinite(values[0]));
        EXPECT_GE(values[0], 0.F);
        EXPECT_LE(values[0], 1.F);
        EXPECT_FLOAT_EQ(window.energy(), 1.F);
    }

    TEST(Window, twoSamplesWindow)
    {
        const HannWindow<2, float> window;
        const auto values{ window.values() };

        EXPECT_FLOAT_EQ(values[0], 0.F);
        EXPECT_FLOAT_EQ(values[1], 0.F);
        EXPECT_FLOAT_EQ(window.energy(), 0.F);
    }

    TEST(Window, coefficientsAreFiniteAndInRange)
    {
        const HannWindow<17, float> window;

        for (float v : window.values())
        {
            EXPECT_TRUE(std::isfinite(v));
            EXPECT_GE(v, 0.F);
            EXPECT_LE(v, 1.F);
        }

        EXPECT_GT(window.energy(), 0.F);
    }

    TEST(Window, symmetric)
    {
        const HannWindow<31, float> window;
        const auto values{ window.values() };

        for (std::size_t i{}; i < values.size() / 2; ++i)
            EXPECT_NEAR(values[i], values[values.size() - 1 - i], 1e-6F);
    }

    TEST(Window, applyUsesPrecomputedCoefficients)
    {
        constexpr std::size_t size{ 8 };
        const HannWindow<size, float> window;

        std::vector<float> input(size);
        for (std::size_t i{}; i < size; ++i)
            input[i] = static_cast<float>(i + 1);

        std::vector<float> output(size);
        window.apply(std::span<const float, size>{ input.data(), input.size() },
                     std::span<float, size>{ output.data(), output.size() });

        const auto coefficients{ window.values() };
        for (std::size_t i{}; i < size; ++i)
            EXPECT_FLOAT_EQ(output[i], input[i] * coefficients[i]);
    }
} // namespace lms::math::tests