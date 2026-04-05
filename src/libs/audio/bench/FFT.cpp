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

#include <benchmark/benchmark.h>

#include "features/AlignedHeapArray.hpp"
#include "features/IFFT.hpp"

namespace lms::audio::features::benchs
{
    namespace
    {
        std::vector<float> generateTestSignal(std::size_t n)
        {
            std::vector<float> data(n);

            for (std::size_t i{}; i < n; ++i)
                data[i] = std::sinf(2.F * std::numbers::pi_v<float> * i / n);

            return data;
        }
    } // namespace

    void BM_FFT(benchmark::State& state)
    {
        const std::size_t n{ static_cast<std::size_t>(state.range(0)) };
        const std::vector<float> inputSignal{ generateTestSignal(n) };

        auto fft{ createRealFFTPlan(n) };
        lms::audio::features::AlignedHeapArray<float, IRealFFTPlan::minBufferAlignment> input{ n };
        lms::audio::features::AlignedHeapArray<std::complex<float>, IRealFFTPlan::minBufferAlignment> output{ fft->getOutputSize() };

        // Fill input buffer with test signal (simulate real use case where input changes every frame)
        std::copy(inputSignal.begin(), inputSignal.end(), input.begin());

        for (auto _ : state)
            fft->apply(input, output);

        state.counters["Samples/s"] = benchmark::Counter{ static_cast<double>(n), benchmark::Counter::kIsIterationInvariantRate };
        state.counters["FFT/s"] = benchmark::Counter{ 1.0, benchmark::Counter::kIsIterationInvariantRate };
    }

    BENCHMARK(BM_FFT)->RangeMultiplier(2)->Range(256, 4096);
} // namespace lms::audio::features::benchs
