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

#include <gtest/gtest.h>

#include "features/DeltaCalculator.hpp"

namespace lms::audio::features::deltaCalculatorTests
{
    constexpr float epsilon{ 1e-5f };

    TEST(DeltaCalculator, valueOnFullWindow)
    {
        DeltaCalculator calc{ 5 };
        EXPECT_FALSE(calc.add(0.F).has_value());
        EXPECT_FALSE(calc.add(1.F).has_value());
        EXPECT_FALSE(calc.add(2.F).has_value());
        EXPECT_FALSE(calc.add(3.F).has_value());
        EXPECT_TRUE(calc.add(4.F).has_value());
    }

    TEST(DeltaCalculator, constant)
    {
        constexpr float constant{ 3.F };
        DeltaCalculator calc{ 5 };
        for (std::size_t i{}; i < calc.getWindowSize(); ++i)
            calc.add(constant);

        const auto delta{ calc.add(constant) };
        EXPECT_TRUE(delta.has_value());
        EXPECT_NEAR(delta.value(), 0.F, epsilon);
    }

    TEST(DeltaCalculator, constantIncrement)
    {
        constexpr float increment{ 1.F };
        DeltaCalculator calc{ 5 };

        float value{};
        for (std::size_t i{}; i < calc.getWindowSize(); ++i)
        {
            value += increment;
            calc.add(static_cast<float>(value));
        }

        for (std::size_t i{}; i < calc.getWindowSize(); ++i)
        {
            value += increment;
            const auto delta{ calc.add(static_cast<float>(value)) };
            ASSERT_TRUE(delta.has_value());
            EXPECT_NEAR(delta.value(), increment, epsilon);
        }
    }

    TEST(DeltaCalculator, reset)
    {
        DeltaCalculator calc{ 5 };
        for (std::size_t i{}; i < calc.getWindowSize(); ++i)
            calc.add(static_cast<float>(i));

        EXPECT_TRUE(calc.add(5.F).has_value());
        calc.reset();
        EXPECT_FALSE(calc.add(10.F).has_value());
    }
} // namespace lms::audio::features::deltaCalculatorTests