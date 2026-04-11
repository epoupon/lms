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

#include <iostream>

#include <benchmark/benchmark.h>
#include <random>

#include "som/Network.hpp"
#include "som/Vector.hpp"

namespace lms::som::benchs
{
    static void BM_SOM_Train(benchmark::State& state)
    {
        constexpr std::size_t dimensionCount{ 80 };
        using Network = Network<dimensionCount>;
        using Vector = Vector<dimensionCount>;

        const Coordinate size{ static_cast<Coordinate>(state.range(0)) };
        constexpr std::size_t epochCount{ 40 };
        constexpr std::size_t datasetSize{ 50 };

        std::minstd_rand networkRandomEngine{ 42 };
        Network initialNetwork{ size, size, networkRandomEngine, 0.F, 1.F };

        std::vector<Vector> dataset;
        dataset.resize(datasetSize);

        {
            std::minstd_rand datasetRandomEngine{ 0 };
            std::uniform_real_distribution<float> distrib{ 0.F, 1.F };
            for (Vector& data : dataset)
            {
                for (auto& value : data)
                    value = distrib(datasetRandomEngine);
            }
        }

        Network network{ dimensionCount, dimensionCount };
        for (auto _ : state)
        {
            state.PauseTiming();
            network = initialNetwork; // determinism
            network.beginTraining(epochCount);
            network.beginNextEpoch();
            state.ResumeTiming();

            for (const auto& input : dataset)
                network.train(input);
        }

        state.SetItemsProcessed(state.iterations() * dataset.size());
    }

    // Benchmark different SOM sizes
    BENCHMARK(BM_SOM_Train)->RangeMultiplier(2)->Range(4, 128);
} // namespace lms::som::benchs
