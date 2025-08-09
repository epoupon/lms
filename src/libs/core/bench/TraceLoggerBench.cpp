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

#include <thread>

#include <benchmark/benchmark.h>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

namespace lms::core
{
    // The trace logger is meant to built/destroyed once
    const Service<logging::ILogger> logger{ logging::createLogger() };
    const Service<tracing::ITraceLogger> traceLogger{ tracing::createTraceLogger(tracing::Level::Overview) };

    static void BM_TraceLogger_Overview(benchmark::State& state)
    {
        for (auto _ : state)
        {
            LMS_SCOPED_TRACE_OVERVIEW("Cat", "Test");
        }
    }

    static void BM_TraceLogger_Overview_withArg(benchmark::State& state)
    {
        for (auto _ : state)
        {
            LMS_SCOPED_TRACE_OVERVIEW_WITH_ARG("Cat", "Test", "ArgType", "My arg that can be very very long, and even as long as needed");
        }
    }

    static void BM_TraceLogger_Detailed(benchmark::State& state)
    {
        for (auto _ : state)
        {
            // Should do nothing
            LMS_SCOPED_TRACE_DETAILED("Cat", "Test");
        }
    }

    static void BM_TraceLogger_Detailed_withArg(benchmark::State& state)
    {
        auto someExpensiveArgComputation{ []() -> std::string {
            std::this_thread::sleep_for(std::chrono::microseconds{ 1 });
            return "foo";
        } };

        for (auto _ : state)
        {
            // Should do nothing (and cost nothing)
            LMS_SCOPED_TRACE_DETAILED_WITH_ARG("Cat", "Test", "ArgType", someExpensiveArgComputation());
        }
    }

    BENCHMARK(BM_TraceLogger_Overview)->Threads(1)->Threads(std::thread::hardware_concurrency());
    BENCHMARK(BM_TraceLogger_Overview_withArg)->Threads(1)->Threads(std::thread::hardware_concurrency());
    BENCHMARK(BM_TraceLogger_Detailed)->Threads(1)->Threads(std::thread::hardware_concurrency());
    BENCHMARK(BM_TraceLogger_Detailed_withArg)->Threads(1)->Threads(std::thread::hardware_concurrency());

} // namespace lms::core

BENCHMARK_MAIN();