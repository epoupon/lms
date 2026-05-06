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

#include <benchmark/benchmark.h>

#include "core/AlignedHeapArray.hpp"

#include "math/FFT.hpp"

namespace lms::math::benchs
{
    namespace
    {
        template<typename FloatType>
        std::vector<FloatType> generateTestSignal(std::size_t n)
        {
            std::vector<FloatType> data(n);

            for (std::size_t i{}; i < n; ++i)
                data[i] = std::sin(static_cast<FloatType>(2) * std::numbers::pi_v<FloatType> * static_cast<FloatType>(i) / static_cast<FloatType>(n));

            return data;
        }
    } // namespace

    template<std::size_t N, typename FloatType>
    void BM_FFT(benchmark::State& state)
    {
        const std::vector<FloatType> inputSignal{ generateTestSignal<FloatType>(N) };

        FixedRealFFTPlan<N, FloatType> fft;
        core::AlignedHeapArray<FloatType, FixedRealFFTPlan<N>::minBufferAlignment> input{ N };
        core::AlignedHeapArray<std::complex<FloatType>, FixedRealFFTPlan<N>::minBufferAlignment> output{ fft.getOutputSize() };

        std::copy(inputSignal.begin(), inputSignal.end(), input.begin());

        for (auto _ : state)
            fft.apply({ input.data(), input.size() }, { output.data(), output.size() });

        state.counters["Samples/s"] = benchmark::Counter{ static_cast<double>(N), benchmark::Counter::kIsIterationInvariantRate };
        state.counters["FFT/s"] = benchmark::Counter{ 1.0, benchmark::Counter::kIsIterationInvariantRate };
    }

    BENCHMARK(BM_FFT<512, float>);
    BENCHMARK(BM_FFT<1024, float>);
    BENCHMARK(BM_FFT<2048, float>);
    BENCHMARK(BM_FFT<512, double>);
    BENCHMARK(BM_FFT<1024, double>);
    BENCHMARK(BM_FFT<2048, double>);

} // namespace lms::math::benchs
