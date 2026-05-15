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
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "core/AlignedHeapArray.hpp"

#include "math/FFT.hpp"
#include "math/Window.hpp"

namespace lms::math::fftTests
{
    constexpr float epsilon{ 1e-3F };

    namespace
    {
        std::size_t getRealFFTOutputSize(std::size_t inputSize)
        {
            return inputSize / 2 + 1;
        }

        std::vector<std::complex<float>> computeRealDFT(const std::vector<float>& input)
        {
            const std::size_t N{ input.size() };
            std::vector<std::complex<float>> output(getRealFFTOutputSize(N));

            for (std::size_t k{}; k <= N / 2; ++k)
            {
                std::complex<double> sum{ 0.0, 0.0 };
                for (std::size_t n{}; n < N; ++n)
                {
                    const double angle{ -2.0 * std::numbers::pi_v<double> * static_cast<double>(k) * static_cast<double>(n) / static_cast<double>(N) };
                    std::complex<double> w{ std::cos(angle), std::sin(angle) };
                    sum += static_cast<double>(input[n]) * w;
                }
                output[k] = { static_cast<float>(sum.real()), static_cast<float>(sum.imag()) };
            }

            return output;
        }
    } // namespace

    TEST(FFT, impulse)
    {
        constexpr std::size_t N{ 8 };
        const std::initializer_list<float> inputSignal{ 1.F, 0.F, 0.F, 0.F, 0.F, 0.F, 0.F, 0.F };
        const auto expected{ computeRealDFT(inputSignal) };

        FixedRealFFTPlan<N> plan;

        core::AlignedHeapArray<float, FixedRealFFTPlan<N>::minBufferAlignment> input{ N };
        core::AlignedHeapArray<std::complex<float>, FixedRealFFTPlan<N>::minBufferAlignment> output{ getRealFFTOutputSize(N) };

        std::copy(inputSignal.begin(), inputSignal.end(), input.begin());
        plan.apply(input, output);
        for (std::size_t i{}; i < output.size(); ++i)
        {
            EXPECT_NEAR(output[i].real(), expected[i].real(), epsilon);
            EXPECT_NEAR(output[i].imag(), expected[i].imag(), epsilon);
        }
    }

    TEST(FFT, realForwardMatchesReference)
    {
        constexpr std::size_t N{ 64 };

        std::vector<float> inputSignal(N);
        for (std::size_t i{}; i < N; ++i)
        {
            inputSignal[i] = std::sin(2.F * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(N))
                           + 0.25F * std::sin(6.F * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(N));
        }

        const auto expected{ computeRealDFT(inputSignal) };

        FixedRealFFTPlan<N> plan;
        core::AlignedHeapArray<float, FixedRealFFTPlan<N>::minBufferAlignment> input{ N };
        core::AlignedHeapArray<std::complex<float>, FixedRealFFTPlan<N>::minBufferAlignment> output{ getRealFFTOutputSize(N) };
        std::copy(inputSignal.begin(), inputSignal.end(), input.begin());

        plan.apply({ input.data(), input.size() }, { output.data(), output.size() });

        for (std::size_t i{}; i < output.size(); ++i)
        {
            EXPECT_NEAR(output[i].real(), expected[i].real(), epsilon);
            EXPECT_NEAR(output[i].imag(), expected[i].imag(), epsilon);
        }
    }

    TEST(FFT, singleFrequencyBin)
    {
        constexpr std::size_t N{ 64 };

        for (std::size_t k{ 1 }; k < N / 2; ++k)
        {
            std::vector<float> inputSignal(N);
            for (std::size_t n{}; n < N; ++n)
                inputSignal[n] = std::sin(2.F * std::numbers::pi_v<float> * static_cast<float>(k) * static_cast<float>(n) / static_cast<float>(N));

            const auto expected{ computeRealDFT(inputSignal) };

            FixedRealFFTPlan<N> plan;
            core::AlignedHeapArray<float, FixedRealFFTPlan<N>::minBufferAlignment> input{ N };
            core::AlignedHeapArray<std::complex<float>, FixedRealFFTPlan<N>::minBufferAlignment> output{ getRealFFTOutputSize(N) };
            std::copy(inputSignal.begin(), inputSignal.end(), input.begin());

            plan.apply({ input.data(), input.size() }, { output.data(), output.size() });

            for (std::size_t i{}; i < output.size(); ++i)
            {
                if (i == k)
                    EXPECT_GT(std::abs(output[i]), 10.F);
                else
                    EXPECT_NEAR(std::abs(output[i]), std::abs(expected[i]), epsilon);
            }
        }
    }

    TEST(FFT, forwardIsUnnormalized)
    {
        constexpr std::size_t N{ 64 };

        FixedRealFFTPlan<N> plan;
        core::AlignedHeapArray<float, FixedRealFFTPlan<N>::minBufferAlignment> input{ N };
        core::AlignedHeapArray<std::complex<float>, FixedRealFFTPlan<N>::minBufferAlignment> output{ getRealFFTOutputSize(N) };
        std::fill(input.begin(), input.end(), 1.F);

        plan.apply({ input.data(), input.size() }, { output.data(), output.size() });

        EXPECT_NEAR(output[0].real(), static_cast<float>(N), epsilon);
    }

    TEST(FFT, parseval)
    {
        constexpr std::size_t N{ 64 };

        std::vector<float> inputSignal(N);
        for (std::size_t i{}; i < N; ++i)
            inputSignal[i] = std::sin(static_cast<float>(i));

        float timeEnergy{};
        for (const auto value : inputSignal)
            timeEnergy += value * value;

        FixedRealFFTPlan<N> plan;
        core::AlignedHeapArray<float, FixedRealFFTPlan<N>::minBufferAlignment> input{ N };
        core::AlignedHeapArray<std::complex<float>, FixedRealFFTPlan<N>::minBufferAlignment> output{ getRealFFTOutputSize(N) };
        std::copy(inputSignal.begin(), inputSignal.end(), input.begin());

        plan.apply({ input.data(), input.size() }, { output.data(), output.size() });

        float freqEnergy{};
        freqEnergy += std::norm(output[0]);
        freqEnergy += std::norm(output[N / 2]);
        for (std::size_t k{ 1 }; k < N / 2; ++k)
            freqEnergy += 2.F * std::norm(output[k]);

        EXPECT_NEAR(timeEnergy, freqEnergy / static_cast<float>(N), epsilon);
    }

    TEST(FFT, parsevalWithWindow)
    {
        constexpr std::size_t N{ 64 };

        std::vector<float> inputSignal(N);
        for (std::size_t n{}; n < N; ++n)
            inputSignal[n] = std::sin(2.F * std::numbers::pi_v<float> * static_cast<float>(n) / static_cast<float>(N));

        const math::HannWindow<N, float> window;
        const float windowEnergy{ window.energy() };

        std::vector<float> windowedInput(N);
        window.apply(std::span<const float, N>{ inputSignal.data(), inputSignal.size() },
                     std::span<float, N>{ windowedInput.data(), windowedInput.size() });

        float E_time{};
        for (float x : windowedInput)
            E_time += x * x;
        E_time /= windowEnergy;

        FixedRealFFTPlan<N> plan;
        core::AlignedHeapArray<float, FixedRealFFTPlan<N>::minBufferAlignment> input{ N };
        core::AlignedHeapArray<std::complex<float>, FixedRealFFTPlan<N>::minBufferAlignment> output{ getRealFFTOutputSize(N) };
        std::copy(windowedInput.begin(), windowedInput.end(), input.begin());
        plan.apply({ input.data(), input.size() }, { output.data(), output.size() });

        float E_freq{};
        E_freq += std::norm(output[0]);
        E_freq += std::norm(output[N / 2]);
        for (std::size_t k{ 1 }; k < N / 2; ++k)
            E_freq += 2.F * std::norm(output[k]);
        E_freq /= (windowEnergy * static_cast<float>(N));

        EXPECT_NEAR(E_time, E_freq, epsilon * E_time) << "Time-domain and frequency-domain energy mismatch after windowing";
    }
} // namespace lms::math::fftTests
