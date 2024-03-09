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

#include <iostream>
#include <thread>
#include <benchmark/benchmark.h>

#include "utils/ILogger.hpp"
#include "utils/IProfiler.hpp"
#include "utils/StreamLogger.hpp"


// Profiler is meant to built/destroyed once
Service<ILogger> logger{ std::make_unique<StreamLogger>(std::cout, StreamLogger::allSeverities) };
Service<profiling::IProfiler> profiler{ profiling::createProfiler(::profiling::Level::Overview) };

static void BM_Profiler_Overview(benchmark::State& state)
{
    for (auto _ : state)
    {
        LMS_SCOPED_PROFILE_OVERVIEW("Cat", "Test");
    }
}

static void BM_Profiler_Detailed(benchmark::State& state)
{
    for (auto _ : state)
    {
        // Should do nothing
        LMS_SCOPED_PROFILE_DETAILED("Cat", "Test");
    }
}

BENCHMARK(BM_Profiler_Overview)->Threads(1)->Threads(std::thread::hardware_concurrency());
BENCHMARK(BM_Profiler_Detailed)->Threads(1)->Threads(std::thread::hardware_concurrency());

BENCHMARK_MAIN();