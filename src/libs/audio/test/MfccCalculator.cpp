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

#include "features/MfccCalculator.hpp"

namespace lms::audio::features::tests
{
    TEST(MfccCalculator, computeMfccFrame)
    {
        constexpr std::size_t melBandCount{ 40 };
        constexpr std::size_t mfccCount{ 13 };
        using Mfcc = MfccCalculator<melBandCount, mfccCount, float>;
        Mfcc mfccCalc;

        std::array<float, melBandCount> logMel;
        logMel.fill(1.F);

        const auto mfcc{ mfccCalc.apply(logMel) };

        constexpr float dctOrthoScale{ std::sqrt(2.F / static_cast<float>(melBandCount)) };
        EXPECT_FLOAT_EQ(mfcc[0], std::sqrt(0.5F) * dctOrthoScale * static_cast<float>(melBandCount));
    }

    TEST(MfccCalculator, computeMfccFrameFlatSpectrum)
    {
        constexpr std::size_t melBandCount{ 40 };
        constexpr std::size_t mfccCount{ 13 };
        using Mfcc = MfccCalculator<melBandCount, mfccCount, float>;
        Mfcc mfccCalc;

        std::array<float, melBandCount> logMel{};

        const auto mfcc{ mfccCalc.apply(logMel) };
        for (const auto val : mfcc)
            EXPECT_FLOAT_EQ(val, 0.F);
    }

    TEST(MfccCalculator, computeMfccFrame_ConstantSignal)
    {
        constexpr std::size_t melBandCount{ 40 };
        constexpr std::size_t mfccCount{ 13 };
        using Mfcc = MfccCalculator<melBandCount, mfccCount, float>;
        Mfcc mfccCalc;

        std::array<float, melBandCount> logMel;
        logMel.fill(1.F);

        const auto mfcc{ mfccCalc.apply(logMel) };

        // DC component should dominate (k=0)
        EXPECT_GT(std::abs(mfcc[0]), 0.F);

        // higher coefficients should be relatively small
        for (std::size_t k{ 1 }; k < mfccCount; ++k)
            EXPECT_NEAR(mfcc[k], 0.F, 1e-3F);
    }

    TEST(MfccCalculator, computeMfccFrame_FiniteValues)
    {
        constexpr std::size_t melBandCount{ 40 };
        constexpr std::size_t mfccCount{ 13 };

        using Mfcc = MfccCalculator<melBandCount, mfccCount, float>;
        Mfcc mfccCalc;

        std::array<float, melBandCount> logMel;
        logMel.fill(100.F);

        const auto mfcc{ mfccCalc.apply(logMel) };

        for (auto val : mfcc)
            EXPECT_TRUE(std::isfinite(val));
    }

    TEST(MfccCalculator, computeMfccFrame_AlternatingSignal)
    {
        constexpr std::size_t melBandCount{ 40 };
        constexpr std::size_t mfccCount{ 13 };

        using Mfcc = MfccCalculator<melBandCount, mfccCount, float>;
        Mfcc mfccCalc;

        std::array<float, melBandCount> logMel;
        for (std::size_t i{}; i < logMel.size(); ++i)
            logMel[i] = (i % 2 == 0) ? 1.F : -1.F;

        const auto mfcc{ mfccCalc.apply(logMel) };

        // only sanity checks, not exact values
        for (auto val : mfcc)
            EXPECT_TRUE(std::isfinite(val));

        // energy should not all collapse to zero
        float energy = 0.F;
        for (auto v : mfcc)
            energy += std::abs(v);

        EXPECT_GT(energy, 0.F);
    }
} // namespace lms::audio::features::tests