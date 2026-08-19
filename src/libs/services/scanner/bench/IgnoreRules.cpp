/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <string>

#include <benchmark/benchmark.h>

#include "IgnoreRules.hpp"

namespace lms::scanner::benchmarks
{
    namespace
    {
        std::filesystem::path makeDeepPath(std::size_t depth)
        {
            std::filesystem::path path;
            for (std::size_t i{}; i < depth; ++i)
                path /= "artist_" + std::to_string(i);

            path /= "track.flac";
            return path;
        }

        std::string makeManyRules(std::size_t count)
        {
            std::string content;
            for (std::size_t i{}; i < count; ++i)
                content += "*.rule_" + std::to_string(i) + "\n";

            return content;
        }

        void BM_IgnoreRules_isIgnored_ShallowNoMatch(benchmark::State& state)
        {
            const IgnoreRules rules{ "*.nfo\ncovers/\n" };

            for (auto _ : state)
                benchmark::DoNotOptimize(rules.isIgnored("track.flac", IgnoreRules::IsDirectory{ false }));
        }

        void BM_IgnoreRules_isIgnored_ShallowMatch(benchmark::State& state)
        {
            const IgnoreRules rules{ "*.nfo\ncovers/\n" };

            for (auto _ : state)
                benchmark::DoNotOptimize(rules.isIgnored("track.nfo", IgnoreRules::IsDirectory{ false }));
        }

        // Worst case for the ancestor walk: no rule ever matches, so every
        // ancestor of a deep path gets fully evaluated on every call.
        void BM_IgnoreRules_isIgnored_DeepPath_NoMatch(benchmark::State& state)
        {
            const IgnoreRules rules{ "*.nfo\ncovers/\n" };
            const std::filesystem::path path{ makeDeepPath(static_cast<std::size_t>(state.range(0))) };

            for (auto _ : state)
                benchmark::DoNotOptimize(rules.isIgnored(path, IgnoreRules::IsDirectory{ false }));
        }

        // Best case: the leaf itself matches, so the ancestor walk exits on its first iteration.
        void BM_IgnoreRules_isIgnored_DeepPath_LeafMatch(benchmark::State& state)
        {
            const IgnoreRules rules{ "*.flac\n" };
            const std::filesystem::path path{ makeDeepPath(static_cast<std::size_t>(state.range(0))) };

            for (auto _ : state)
                benchmark::DoNotOptimize(rules.isIgnored(path, IgnoreRules::IsDirectory{ false }));
        }

        // Combines a deep path with a large rule set: every level of the
        // ancestor walk pays the full per-rule fnmatch fold.
        void BM_IgnoreRules_isIgnored_DeepPath_ManyRules_NoMatch(benchmark::State& state)
        {
            const IgnoreRules rules{ makeManyRules(static_cast<std::size_t>(state.range(1))) };
            const std::filesystem::path path{ makeDeepPath(static_cast<std::size_t>(state.range(0))) };

            for (auto _ : state)
                benchmark::DoNotOptimize(rules.isIgnored(path, IgnoreRules::IsDirectory{ false }));
        }
    } // namespace

    BENCHMARK(BM_IgnoreRules_isIgnored_ShallowNoMatch);
    BENCHMARK(BM_IgnoreRules_isIgnored_ShallowMatch);
    BENCHMARK(BM_IgnoreRules_isIgnored_DeepPath_NoMatch)->Arg(4)->Arg(8);
    BENCHMARK(BM_IgnoreRules_isIgnored_DeepPath_LeafMatch)->Arg(4)->Arg(8);
    BENCHMARK(BM_IgnoreRules_isIgnored_DeepPath_ManyRules_NoMatch)->Args({ 4, 10 })->Args({ 8, 20 });

} // namespace lms::scanner::benchmarks
