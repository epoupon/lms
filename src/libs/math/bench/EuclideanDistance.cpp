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
#include <random>

#include "core/Random.hpp"

#include "math/EuclideanDistance.hpp"

namespace lms::core::benchs
{
    template<std::size_t Size>
    static void BM_SquaredEuclideanDistance(benchmark::State& state)
    {
        std::minstd_rand randomEngine{ 0 };

        math::Vector<Size, float> vec1;
        math::Vector<Size, float> vec2;

        core::random::fillContainer(randomEngine, vec1, 0.F, 1.F);
        core::random::fillContainer(randomEngine, vec2, 0.F, 1.F);

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(math::computeEuclideanSquaredDistance(vec1, vec2));
        }

        state.SetItemsProcessed(state.iterations() * Size);
    }

    template<std::size_t Size>
    static void BM_SquaredEuclideanDistanceWithWeights(benchmark::State& state)
    {
        std::minstd_rand randomEngine{ 0 };

        math::Vector<Size, float> vec1;
        math::Vector<Size, float> vec2;
        math::Vector<Size, float> weights;

        core::random::fillContainer(randomEngine, vec1, 0.F, 1.F);
        core::random::fillContainer(randomEngine, vec2, 0.F, 1.F);
        core::random::fillContainer(randomEngine, weights, 0.F, 1.F);

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(math::computeEuclideanSquaredDistanceWithWeights(vec1, vec2, weights));
        }

        state.SetItemsProcessed(state.iterations() * Size);
    }

    BENCHMARK_TEMPLATE(BM_SquaredEuclideanDistance, 4);
    BENCHMARK_TEMPLATE(BM_SquaredEuclideanDistance, 50);
    BENCHMARK_TEMPLATE(BM_SquaredEuclideanDistance, 160);
    BENCHMARK_TEMPLATE(BM_SquaredEuclideanDistanceWithWeights, 4);
    BENCHMARK_TEMPLATE(BM_SquaredEuclideanDistanceWithWeights, 50);
    BENCHMARK_TEMPLATE(BM_SquaredEuclideanDistanceWithWeights, 160);
} // namespace lms::core::benchs
