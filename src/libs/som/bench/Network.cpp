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
#include <span>

#include <benchmark/benchmark.h>

#include "som/Network.hpp"
#include "som/Trainer.hpp"
#include "som/Vector.hpp"

namespace lms::som::benchs
{
    constexpr std::size_t dimensionCount{ 10 };
    using Network = Network<dimensionCount>;
    using Vector = Vector<dimensionCount>;

    constexpr Vector::value_type minValue{ 0.F };
    constexpr Vector::value_type maxValue{ 1.F };

    constexpr std::size_t epochCount{ 10 };
    constexpr std::size_t datasetSize{ 100 };

    Network createRandomNetwork(Coordinate width, Coordinate height)
    {
        std::minstd_rand randomEngine{ 42 };

        Network network{ width, height };
        network.randomize(randomEngine, minValue, maxValue);

        return network;
    }

    std::vector<Vector> createRandomDataset()
    {
        std::vector<Vector> dataset;
        dataset.resize(datasetSize);

        std::minstd_rand datasetRandomEngine{ 0 };
        std::uniform_real_distribution<float> distrib{ minValue, maxValue };
        for (Vector& data : dataset)
        {
            for (auto& value : data)
                value = distrib(datasetRandomEngine);
        }

        return dataset;
    }

    float computeQuantizationError(const Network& network, std::span<const Vector> dataset)
    {
        float quantizationError{};
        for (const Vector& inputVector : dataset)
        {
            const Vector& bestMatchingNeuron{ network.getNeuron(network.getBestMatchingNeuron(inputVector)) };
            quantizationError += inputVector.computeEuclideanSquaredDistance(bestMatchingNeuron);
        }
        return quantizationError;
    }

    void BM_SOM_Train(benchmark::State& state)
    {
        const Coordinate size{ static_cast<Coordinate>(state.range(0)) };

        const Network initialNetwork{ createRandomNetwork(size, size) };
        const std::vector<Vector> dataset{ createRandomDataset() };

        Network network{ dimensionCount, dimensionCount };
        for (auto _ : state)
        {
            state.PauseTiming();
            network = initialNetwork; // determinism
            state.ResumeTiming();

            Trainer trainer{ network, TrainerParams{ .epochCount = epochCount } };

            for (std::size_t epoch{}; epoch < epochCount; ++epoch)
            {
                trainer.beginEpoch();

                for (const Vector& input : dataset)
                    trainer.train(input);
            }
        }

        state.counters["QE"] = computeQuantizationError(network, dataset);
        state.SetItemsProcessed(state.iterations() * dataset.size() * epochCount);
    }

    // Benchmark different SOM sizes
    BENCHMARK(BM_SOM_Train)->RangeMultiplier(2)->Range(4, 128);
} // namespace lms::som::benchs
