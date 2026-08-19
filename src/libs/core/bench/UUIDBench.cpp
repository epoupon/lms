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

#include "core/UUID.hpp"

namespace lms::core::benchs
{
    static void BM_UUID_fromString(benchmark::State& state)
    {
        for (auto _ : state)
            benchmark::DoNotOptimize(UUID::fromString("3f51c839-bee2-4e9d-a7b7-0693e45178fc"));
    }

    static void BM_UUID_fromString_invalid(benchmark::State& state)
    {
        for (auto _ : state)
            benchmark::DoNotOptimize(UUID::fromString("not-a-valid-uuid-string-at-all-xx"));
    }

    static void BM_UUID_toString(benchmark::State& state)
    {
        const UUID uuid{ *UUID::fromString("3f51c839-bee2-4e9d-a7b7-0693e45178fc") };
        for (auto _ : state)
            benchmark::DoNotOptimize(uuid.toString());
    }

    static void BM_UUID_generate(benchmark::State& state)
    {
        for (auto _ : state)
            benchmark::DoNotOptimize(UUID::generate());
    }

    BENCHMARK(BM_UUID_fromString);
    BENCHMARK(BM_UUID_fromString_invalid);
    BENCHMARK(BM_UUID_toString);
    BENCHMARK(BM_UUID_generate);
} // namespace lms::core::benchs
