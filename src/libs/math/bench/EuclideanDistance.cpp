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

#include <random>
#include <vector>

#include <benchmark/benchmark.h>

#include "core/Random.hpp"

#include "math/EuclideanDistance.hpp"

namespace lms::core::benchs
{
    static void BM_SquaredEuclideanDistance(benchmark::State& state)
    {
        const auto dimensionCount{ static_cast<std::size_t>(state.range(0)) };

        std::minstd_rand randomEngine{ 0 };

        std::vector<float> vec1(dimensionCount);
        std::vector<float> vec2(dimensionCount);

        random::fillContainer(randomEngine, vec1, 0.F, 1.F);
        random::fillContainer(randomEngine, vec2, 0.F, 1.F);

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(math::computeEuclideanSquaredDistance(vec1.data(), vec2.data(), dimensionCount));
        }

        state.SetItemsProcessed(state.iterations() * dimensionCount);
    }

    static void BM_SquaredEuclideanDistanceWithWeights(benchmark::State& state)
    {
        const auto dimensionCount{ static_cast<std::size_t>(state.range(0)) };

        std::minstd_rand randomEngine{ 0 };

        std::vector<float> vec1(dimensionCount);
        std::vector<float> vec2(dimensionCount);
        std::vector<float> weights(dimensionCount);

        random::fillContainer(randomEngine, vec1, 0.F, 1.F);
        random::fillContainer(randomEngine, vec2, 0.F, 1.F);
        random::fillContainer(randomEngine, weights, 0.F, 1.F);

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(math::computeEuclideanSquaredDistanceWithWeights(vec1.data(), vec2.data(), weights.data(), dimensionCount));
        }

        state.SetItemsProcessed(state.iterations() * dimensionCount);
    }

    BENCHMARK(BM_SquaredEuclideanDistance)->Arg(4)->Arg(50)->Arg(160);
    BENCHMARK(BM_SquaredEuclideanDistanceWithWeights)->Arg(4)->Arg(50)->Arg(160);
} // namespace lms::core::benchs
