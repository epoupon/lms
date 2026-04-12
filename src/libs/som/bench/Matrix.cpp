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

#include <benchmark/benchmark.h>

#include "som/Matrix.hpp"
#include "som/Vector.hpp"

namespace lms::som::benchs
{
    namespace
    {
        constexpr std::size_t dimensionCount{ 80 };
        using Value = Vector<dimensionCount, float>;
        using randomEngine = std::minstd_rand;

        void fillWithRandom(randomEngine& randomEngine, Value& value)
        {
            std::uniform_real_distribution<float> distrib{ 0.F, 1.F };
            for (auto& v : value)
                v = distrib(randomEngine);
        }

        void fillWithRandom(randomEngine& randomEngine, Matrix<Value>& matrix)
        {
            for (std::size_t x{}; x < matrix.getWidth(); ++x)
            {
                for (std::size_t y{}; y < matrix.getHeight(); ++y)
                    fillWithRandom(randomEngine, matrix.get(x, y));
            }
        }
    } // namespace

    static void BM_Matrix_GetMinDistance_SquaredEuclideanDistance(benchmark::State& state)
    {
        const auto matrixSize{ static_cast<Coordinate>(state.range(0)) };
        Matrix<Value> matrix{ matrixSize, matrixSize };

        std::minstd_rand randomEngine{ 0 };
        fillWithRandom(randomEngine, matrix);

        const Value ref{ 0.5F };

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(matrix.getPositionMinDistance(SquaredEuclideanDistance{ ref }));
        }

        state.SetItemsProcessed(state.iterations() * matrixSize * matrixSize);
    }

    static void BM_Matrix_GetMinDistance_SquaredEuclideanDistanceWithWeights(benchmark::State& state)
    {
        const auto matrixSize{ static_cast<Coordinate>(state.range(0)) };
        Matrix<Value> matrix{ matrixSize, matrixSize };
        Value weights{ 1.F };

        std::minstd_rand randomEngine{ 0 };
        fillWithRandom(randomEngine, matrix);

        const Value ref{ 0.5F };

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(matrix.getPositionMinDistance(SquaredEuclideanDistanceWithWeights{ ref, weights }));
        }

        state.SetItemsProcessed(state.iterations() * matrixSize * matrixSize);
    }

    BENCHMARK(BM_Matrix_GetMinDistance_SquaredEuclideanDistance)->RangeMultiplier(2)->Range(4, 32);
    BENCHMARK(BM_Matrix_GetMinDistance_SquaredEuclideanDistanceWithWeights)->RangeMultiplier(2)->Range(4, 32);
} // namespace lms::som::benchs
