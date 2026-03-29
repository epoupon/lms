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
#include <complex>

#include <gtest/gtest.h>

#include "features/IFFT.hpp"
#include "features/Window.hpp"

namespace lms::audio::features::fftTests
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
        const std::initializer_list<float> input{ 1.F, 0.F, 0.F, 0.F, 0.F, 0.F, 0.F, 0.F };
        const auto expected{ computeRealDFT(input) };

        auto plan{ createRealFFTPlan(N) };
        std::copy(input.begin(), input.end(), plan->getInputBuffer().begin());
        plan->apply();

        const auto output{ plan->getOutputBuffer() };
        for (std::size_t i{}; i < output.size(); ++i)
        {
            EXPECT_NEAR(output[i].real(), expected[i].real(), epsilon);
            EXPECT_NEAR(output[i].imag(), expected[i].imag(), epsilon);
        }
    }

    TEST(FFT, realForwardMatchesReference)
    {
        constexpr std::size_t N{ 64 };

        std::vector<float> input(N);
        for (std::size_t i{}; i < N; ++i)
        {
            input[i] = std::sin(2.F * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(N))
                     + 0.25F * std::sin(6.F * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(N));
        }

        const auto expected{ computeRealDFT(input) };

        auto plan{ createRealFFTPlan(N) };
        std::copy(input.begin(), input.end(), plan->getInputBuffer().begin());
        plan->apply();

        const auto output{ plan->getOutputBuffer() };
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
            std::vector<float> input(N);
            for (std::size_t n{}; n < N; ++n)
                input[n] = std::sin(2.F * std::numbers::pi_v<float> * static_cast<float>(k) * static_cast<float>(n) / static_cast<float>(N));

            const auto expected{ computeRealDFT(input) };

            auto plan{ createRealFFTPlan(N) };
            std::copy(input.begin(), input.end(), plan->getInputBuffer().begin());
            plan->apply();

            const auto output{ plan->getOutputBuffer() };
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

        auto plan{ createRealFFTPlan(N) };
        std::fill(std::begin(plan->getInputBuffer()), std::end(plan->getInputBuffer()), 1.F);
        plan->apply();

        const auto& output = plan->getOutputBuffer();

        // DC bin should be ~N
        EXPECT_NEAR(output[0].real(), static_cast<float>(N), epsilon);
    }

    TEST(FFT, parseval)
    {
        constexpr std::size_t N{ 64 };

        std::vector<float> input(N);
        for (std::size_t i{}; i < N; ++i)
            input[i] = std::sinf(static_cast<float>(i));

        float timeEnergy{};
        for (const auto value : input)
            timeEnergy += value * value;

        auto plan{ createRealFFTPlan(N) };
        std::copy(input.begin(), input.end(), plan->getInputBuffer().begin());
        plan->apply();

        const auto output{ plan->getOutputBuffer() };
        float freqEnergy{};

        // Parseval for real input with reduced spectrum:
        freqEnergy += std::norm(output[0]);
        freqEnergy += std::norm(output[N / 2]);
        for (std::size_t k{ 1 }; k < N / 2; ++k)
            freqEnergy += 2.F * std::norm(output[k]);

        EXPECT_NEAR(timeEnergy, freqEnergy / static_cast<float>(N), epsilon);
    }

    TEST(FFT, parsevalWithWindow)
    {
        constexpr std::size_t N{ 64 };

        // test signal
        std::vector<float> input(N);
        for (size_t n = 0; n < N; ++n)
            input[n] = std::sinf(2.F * std::numbers::pi_v<float> * n / static_cast<float>(N));

        // Hann window
        std::vector<float> window(N);
        computeHannWindow(window);
        float windowEnergy{};
        for (size_t n = 0; n < N; ++n)
            windowEnergy += window[n] * window[n];

        // Apply window
        std::vector<float> windowedInput(N);
        for (size_t n = 0; n < N; ++n)
            windowedInput[n] = input[n] * window[n];

        // Time-domain energy
        float E_time{};
        for (float x : windowedInput)
            E_time += x * x;
        E_time /= windowEnergy;

        // FFT
        auto plan = createRealFFTPlan(N);
        std::copy(windowedInput.begin(), windowedInput.end(), plan->getInputBuffer().begin());
        plan->apply();
        const auto& outBuf = plan->getOutputBuffer();

        // Frequency-domain energy
        float E_freq{};
        E_freq += std::norm(outBuf[0]);
        E_freq += std::norm(outBuf[N / 2]);
        for (size_t k = 1; k < N / 2; ++k)
            E_freq += 2.F * std::norm(outBuf[k]);
        E_freq /= (windowEnergy * N);

        EXPECT_NEAR(E_time, E_freq, epsilon * E_time) << "Time-domain and frequency-domain energy mismatch after windowing";
    }
} // namespace lms::audio::features::fftTests