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

#include "features/OnsetCalculator.hpp"

namespace lms::audio::features::tests
{
    TEST(OnsetCalculator, FirstFrameReturnsZero)
    {
        OnsetCalculator<4, float> calculator;

        const std::array<float, 4> frame{ 1.F, 2.F, 3.F, 4.F };
        EXPECT_FLOAT_EQ(calculator.apply(frame), 0.F);
    }

    TEST(OnsetCalculator, IdenticalFramesReturnZeroNovelty)
    {
        OnsetCalculator<4, float> calculator;

        const std::array<float, 4> frame{ 1.F, 2.F, 3.F, 4.F };
        static_cast<void>(calculator.apply(frame));
        EXPECT_FLOAT_EQ(calculator.apply(frame), 0.F);
    }

    TEST(OnsetCalculator, PositiveChangesAreAccumulated)
    {
        OnsetCalculator<4, float> calculator;

        const std::array<float, 4> previous{ 1.F, 2.F, 3.F, 4.F };
        const std::array<float, 4> current{ 2.F, 1.F, 5.F, 4.F };

        static_cast<void>(calculator.apply(previous));
        EXPECT_FLOAT_EQ(calculator.apply(current), 3.F);
    }

    TEST(OnsetCalculator, SilenceResetsHistory)
    {
        OnsetCalculator<4, float> calculator;

        const std::array<float, 4> frame{ 1.F, 2.F, 3.F, 4.F };
        const std::array<float, 4> silence{};

        static_cast<void>(calculator.apply(frame));
        EXPECT_FLOAT_EQ(calculator.apply(silence), 0.F);
        EXPECT_FLOAT_EQ(calculator.apply(frame), 0.F);
    }

    TEST(OnsetCalculator, ExplicitResetClearsHistory)
    {
        OnsetCalculator<4, float> calculator;

        const std::array<float, 4> frame1{ 1.F, 2.F, 3.F, 4.F };
        const std::array<float, 4> frame2{ 2.F, 3.F, 4.F, 5.F };

        static_cast<void>(calculator.apply(frame1));
        EXPECT_GT(calculator.apply(frame2), 0.F);

        calculator.reset();
        EXPECT_FLOAT_EQ(calculator.apply(frame2), 0.F);
    }

    TEST(OnsetCalculator, VerySmallEnergyIsHandledAsSilence)
    {
        OnsetCalculator<4, float> calculator;

        const std::array<float, 4> tiny{ 1e-30F, 1e-30F, 1e-30F, 1e-30F }; // avoid denorm dependency
        const std::array<float, 4> frame{ 1.F, 1.F, 1.F, 1.F };

        EXPECT_FLOAT_EQ(calculator.apply(tiny), 0.F);
        EXPECT_FLOAT_EQ(calculator.apply(frame), 0.F);
    }
} // namespace lms::audio::features::tests
