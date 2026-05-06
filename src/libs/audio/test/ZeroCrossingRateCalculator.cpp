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

#include <array>

#include <gtest/gtest.h>

#include "features/ZeroCrossingRateCalculator.hpp"

namespace lms::audio::features::zeroCrossingRateTests
{
    TEST(ZeroCrossingRateCalculator, EmptyInput)
    {
        const ZeroCrossingRateCalculator<float> calculator;

        EXPECT_FLOAT_EQ(calculator.apply({}), 0.F);
    }

    TEST(ZeroCrossingRateCalculator, SingleSample)
    {
        const ZeroCrossingRateCalculator<float> calculator;
        const std::array<float, 1> samples{ 5.F };

        EXPECT_FLOAT_EQ(calculator.apply(samples), 0.F);
    }

    TEST(ZeroCrossingRateCalculator, AlternatingSignal)
    {
        const ZeroCrossingRateCalculator<float> calculator;
        const std::array<float, 4> samples{ 1.F, -1.F, 1.F, -1.F };

        EXPECT_FLOAT_EQ(calculator.apply(samples), 0.75F);
    }

    TEST(ZeroCrossingRateCalculator, NoCrossing)
    {
        const ZeroCrossingRateCalculator<float> calculator;
        const std::array<float, 5> samples{ 1.F, 2.F, 3.F, 2.F, 1.F };

        EXPECT_FLOAT_EQ(calculator.apply(samples), 0.F);
    }

    TEST(ZeroCrossingRateCalculator, ZerosAreHandledAsNonNegative)
    {
        const ZeroCrossingRateCalculator<float> calculator;
        const std::array<float, 5> samples{ 1.F, 0.F, -1.F, 0.F, 1.F };

        EXPECT_NEAR(calculator.apply(samples), 0.4F, 1e-5F);
    }

    TEST(ZeroCrossingRateCalculator, VerySmallAmplitudes)
    {
        const ZeroCrossingRateCalculator<float> calculator;
        const std::array<float, 4> samples{ 1e-10F, -1e-10F, 1e-10F, -1e-10F };

        EXPECT_FLOAT_EQ(calculator.apply(samples), 0.75F);
    }

    TEST(ZeroCrossingRateCalculator, OutputRange)
    {
        const ZeroCrossingRateCalculator<float> calculator;
        constexpr std::array<float, 100> samples{ [] {
            std::array<float, 100> generated{};
            for (std::size_t i{}; i < generated.size(); ++i)
                generated[i] = (i % 3 == 0) ? -1.F : 1.F;
            return generated;
        }() };

        const float zcr{ calculator.apply(samples) };
        EXPECT_GE(zcr, 0.F);
        EXPECT_LE(zcr, 1.F);
    }
} // namespace lms::audio::features::zeroCrossingRateTests
