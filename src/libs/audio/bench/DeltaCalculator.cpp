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

#include "features/DeltaCalculator.hpp"

namespace lms::audio::features::benchs
{
    void BM_DeltaCalculator(benchmark::State& state)
    {
        DeltaCalculator calc{ static_cast<std::size_t>(state.range(0)) };

        float value{};
        for (auto _ : state)
        {
            auto delta = calc.add(value);
            benchmark::DoNotOptimize(delta);
            value += 1.F;
        }

        state.SetItemsProcessed(state.iterations());
    }

    BENCHMARK(BM_DeltaCalculator)->Arg(5)->Arg(9)->Arg(15)->Arg(31);
} // namespace lms::audio::features::benchs
