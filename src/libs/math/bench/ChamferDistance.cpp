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
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

#include "core/Random.hpp"

#include "math/ChamferDistance.hpp"
#include "math/Vector.hpp"

namespace lms::core::benchs
{
    template<std::size_t Size>
    struct BenchDistance
    {
        BenchDistance(const math::Vector<Size, float>& ref)
            : _ref{ ref } {}

        float operator()(const math::Vector<Size, float>& b) const
        {
            float sum{};
            for (std::size_t i{}; i < Size; ++i)
            {
                const float diff{ _ref[i] - b[i] };
                sum += diff * diff;
            }
            return std::sqrt(sum);
        }

        const math::Vector<Size, float>& _ref;
    };

    template<std::size_t VectorSize, std::size_t SetASize, std::size_t SetBSize>
    static void BM_ChamferDistanceAtoB(benchmark::State& state)
    {
        std::minstd_rand randomEngine{ 0 };

        std::vector<math::Vector<VectorSize, float>> vecA;
        std::vector<math::Vector<VectorSize, float>> vecB;
        vecA.reserve(SetASize);
        vecB.reserve(SetBSize);

        for (std::size_t i{}; i < SetASize; ++i)
        {
            auto& vec{ vecA.emplace_back() };
            core::random::fillContainer(randomEngine, vec, 0.F, 1.F);
        }

        for (std::size_t i{}; i < SetBSize; ++i)
        {
            auto& vec{ vecB.emplace_back() };
            core::random::fillContainer(randomEngine, vec, 0.F, 1.F);
        }

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(math::chamferDistanceAtoB<BenchDistance<VectorSize>>(vecA, vecB));
        }

        state.SetItemsProcessed(state.iterations() * SetASize * SetBSize);
    }

    template<std::size_t VectorSize, std::size_t SetASize, std::size_t SetBSize>
    static void BM_SymmetricalChamferDistance(benchmark::State& state)
    {
        std::minstd_rand randomEngine{ 0 };

        std::vector<math::Vector<VectorSize, float>> vecA;
        std::vector<math::Vector<VectorSize, float>> vecB;
        vecA.reserve(SetASize);
        vecB.reserve(SetBSize);

        for (std::size_t i{}; i < SetASize; ++i)
        {
            auto& vec{ vecA.emplace_back() };
            core::random::fillContainer(randomEngine, vec, 0.F, 1.F);
        }

        for (std::size_t i{}; i < SetBSize; ++i)
        {
            auto& vec{ vecB.emplace_back() };
            core::random::fillContainer(randomEngine, vec, 0.F, 1.F);
        }

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(math::symmetricalChamferDistance<BenchDistance<VectorSize>>(vecA, vecB));
        }

        state.SetItemsProcessed(state.iterations() * 2 * SetASize * SetBSize);
    }

    // Benchmarks with different configurations
    BENCHMARK_TEMPLATE(BM_ChamferDistanceAtoB, 128, 10, 10);
    BENCHMARK_TEMPLATE(BM_ChamferDistanceAtoB, 128, 50, 50);
    BENCHMARK_TEMPLATE(BM_ChamferDistanceAtoB, 256, 10, 10);
    BENCHMARK_TEMPLATE(BM_ChamferDistanceAtoB, 256, 50, 50);

    BENCHMARK_TEMPLATE(BM_SymmetricalChamferDistance, 128, 10, 10);
    BENCHMARK_TEMPLATE(BM_SymmetricalChamferDistance, 128, 50, 50);
    BENCHMARK_TEMPLATE(BM_SymmetricalChamferDistance, 256, 10, 10);
    BENCHMARK_TEMPLATE(BM_SymmetricalChamferDistance, 256, 50, 50);
} // namespace lms::core::benchs
