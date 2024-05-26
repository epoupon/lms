/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <random>

#include <benchmark/benchmark.h>

#include "som/Network.hpp"

namespace lms::som
{
    // Benchmark function
    static void BM_Matrix(benchmark::State& state)
    {
        std::minstd_rand randomEngine{ 42 };
        std::uniform_int_distribution distrib{ 0, 1000 };

        Matrix<int> matrix{ static_cast<Coordinate>(state.range(0)), static_cast<Coordinate>(state.range(0)) };

        for (Coordinate x{}; x < matrix.getWidth(); ++x)
        {
            for (Coordinate y{}; y < matrix.getHeight(); ++y)
                matrix.get({ x, y }) = distrib(randomEngine);
        }

        for (auto _ : state)
        {
            // Code inside this loop is measured repeatedly
            const Position pos{ matrix.getPositionMinElement([](int a, int b) { return a < b; }) };
            benchmark::DoNotOptimize(pos);
        }

        // Perform cleanup here if needed
    }

    // Register the benchmark with custom range
    BENCHMARK(BM_Matrix)->Arg(3)->Arg(6)->Arg(12)->Arg(24);
} // namespace lms::som

BENCHMARK_MAIN();
