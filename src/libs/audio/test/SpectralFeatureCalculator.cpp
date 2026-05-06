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

#include <algorithm>
#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "features/SpectralFeatureCalculator.hpp"

namespace lms::audio::features::tests
{
    template<std::size_t N>
    constexpr float maxFrequency(float binWidth)
    {
        return static_cast<float>(N - 1) * binWidth;
    }

    TEST(SpectralFeatureCalculator, basicFrameFeatures)
    {
        constexpr std::size_t N{ 5 };
        constexpr float binWidth{ 10.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> powerSpectrum{ 0.F, 1.F, 2.F, 3.F, 4.F };

        const auto f{ calc.apply(powerSpectrum, binWidth) };

        EXPECT_FLOAT_EQ(f.spectralCentroid, 30.F);
        EXPECT_FLOAT_EQ(f.spectralRolloff, 40.F);
        EXPECT_FLOAT_EQ(f.spectralFlux, 0.F);
    }

    TEST(SpectralFeatureCalculator, zeroSpectrum)
    {
        constexpr std::size_t N{ 10 };
        constexpr float binWidth{ 1.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p{};
        p.fill(0.F);

        const auto f{ calc.apply(p, binWidth) };

        EXPECT_FLOAT_EQ(f.spectralCentroid, 0.F);
        EXPECT_FLOAT_EQ(f.spectralRolloff, 0.F);
        EXPECT_FLOAT_EQ(f.spectralFlux, 0.F);
    }

    TEST(SpectralFeatureCalculator, flatSpectrumBounds)
    {
        constexpr std::size_t N{ 100 };
        constexpr float binWidth{ 1.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p;
        p.fill(1.F);

        const auto f{ calc.apply(p, binWidth) };

        EXPECT_GE(f.spectralCentroid, 0.F);
        EXPECT_LE(f.spectralCentroid, maxFrequency<N>(binWidth));

        EXPECT_GE(f.spectralRolloff, 0.F);
        EXPECT_LE(f.spectralRolloff, maxFrequency<N>(binWidth));

        EXPECT_FLOAT_EQ(f.spectralFlux, 0.F);
    }

    TEST(SpectralFeatureCalculator, singleBinEnergy)
    {
        constexpr std::size_t N{ 10 };
        constexpr float binWidth{ 10.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p{};
        p[7] = 5.F;

        const auto f{ calc.apply(p, binWidth) };

        EXPECT_NEAR(f.spectralCentroid, 70.F, 1e-3F);
        EXPECT_GE(f.spectralRolloff, f.spectralCentroid);
        EXPECT_LE(f.spectralRolloff, maxFrequency<N>(binWidth));
    }

    TEST(SpectralFeatureCalculator, consecutiveFramesFlux)
    {
        constexpr std::size_t N{ 5 };
        constexpr float binWidth{ 10.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        const std::array<float, N> p1{ 1.F, 2.F, 3.F, 2.F, 1.F };
        const auto f1{ calc.apply(p1, binWidth) };
        EXPECT_FLOAT_EQ(f1.spectralFlux, 0.F);

        const std::array<float, N> p2{ 3.F, 1.F, 2.F, 1.F, 3.F };
        const auto f2{ calc.apply(p2, binWidth) };
        EXPECT_GT(f2.spectralFlux, 0.F);

        const auto f3{ calc.apply(p2, binWidth) };
        EXPECT_FLOAT_EQ(f3.spectralFlux, 0.F);
    }

    TEST(SpectralFeatureCalculator, consecutiveZeroFrames)
    {
        constexpr std::size_t N{ 5 };
        constexpr float binWidth{ 10.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p{};
        p.fill(0.F);

        for (std::size_t i{}; i < 10; ++i)
        {
            const auto f{ calc.apply(p, binWidth) };
            EXPECT_FLOAT_EQ(f.spectralFlux, 0.F);
        }
    }

    TEST(SpectralFeatureCalculator, highEnergyStability)
    {
        constexpr std::size_t N{ 50 };
        constexpr float binWidth{ 1.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p;
        p.fill(1e6F);

        const auto f{ calc.apply(p, binWidth) };

        EXPECT_GE(f.spectralCentroid, 0.F);
        EXPECT_LE(f.spectralCentroid, maxFrequency<N>(binWidth));

        EXPECT_GE(f.spectralRolloff, 0.F);
        EXPECT_LE(f.spectralRolloff, maxFrequency<N>(binWidth));

        EXPECT_FLOAT_EQ(f.spectralFlux, 0.F);
    }

    TEST(SpectralFeatureCalculator, lowEnergyStability)
    {
        constexpr std::size_t N{ 5 };
        constexpr float binWidth{ 1.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p;
        std::fill(p.begin(), p.end(), 1e-30F); // avoid denorm dependency

        const auto f{ calc.apply(p, binWidth) };

        EXPECT_GE(f.spectralCentroid, 0.F);
        EXPECT_LE(f.spectralCentroid, maxFrequency<N>(binWidth));

        EXPECT_GE(f.spectralRolloff, 0.F);
        EXPECT_LE(f.spectralRolloff, maxFrequency<N>(binWidth));

        EXPECT_FLOAT_EQ(f.spectralFlux, 0.F);
    }

    TEST(SpectralFeatureCalculator, scaleInvariance)
    {
        constexpr std::size_t N{ 20 };
        constexpr float binWidth{ 2.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p;
        for (std::size_t i{}; i < N; ++i)
            p[i] = static_cast<float>(i + 1);

        const auto f1{ calc.apply(p, binWidth) };

        for (auto& v : p)
            v *= 1000.F;

        const auto f2{ calc.apply(p, binWidth) };

        EXPECT_NEAR(f1.spectralCentroid, f2.spectralCentroid, 1e-3F);
        EXPECT_NEAR(f1.spectralRolloff, f2.spectralRolloff, 1e-3F);
    }

    TEST(SpectralFeatureCalculator, noisePattern)
    {
        constexpr std::size_t N{ 100 };
        constexpr float binWidth{ 0.1F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p{};
        for (std::size_t i{}; i < N; ++i)
            p[i] = static_cast<float>(i % 7 + 1);

        const auto f{ calc.apply(p, binWidth) };

        EXPECT_GE(f.spectralCentroid, 0.F);
        EXPECT_LE(f.spectralCentroid, maxFrequency<N>(binWidth));

        EXPECT_GE(f.spectralRolloff, f.spectralCentroid);
        EXPECT_LE(f.spectralRolloff, maxFrequency<N>(binWidth));
    }

    TEST(SpectralFeatureCalculator, continuity)
    {
        constexpr std::size_t N{ 10 };
        constexpr float binWidth{ 1.F };
        using Calc = SpectralFeatureCalculator<N, float>;

        Calc calc;

        std::array<float, N> p1{};
        std::array<float, N> p2{};

        for (std::size_t i{}; i < N; ++i)
        {
            p1[i] = static_cast<float>(i + 1);
            p2[i] = p1[i] + 0.01F; // small perturbation
        }

        const auto f1{ calc.apply(p1, binWidth) };
        const auto f2{ calc.apply(p2, binWidth) };

        EXPECT_LT(std::abs(f2.spectralCentroid - f1.spectralCentroid), 1.F);
        EXPECT_LT(std::abs(f2.spectralRolloff - f1.spectralRolloff), 1.F);
    }

} // namespace lms::audio::features::tests