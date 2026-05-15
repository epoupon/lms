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

#include <array>
#include <cstdlib>
#include <filesystem>
#include <random>

#include <benchmark/benchmark.h>

#include "musicnn/MusicNNModel.hpp"

namespace lms::audio::musicnn::benchmarks
{
    namespace
    {
        std::filesystem::path getMusicNNModelPathFromEnv()
        {
            const char* p{ std::getenv("LMS_MUSICNN_MODEL") };
            return p ? std::filesystem::path{ p } : std::filesystem::path{};
        }

        std::array<float, MusicNNModel::inputFrames * MusicNNModel::inputBands> makeRandomPatch()
        {
            std::minstd_rand rng{ 42 };
            std::uniform_real_distribution<float> dist{ 0.F, 1.F };
            std::array<float, MusicNNModel::inputFrames * MusicNNModel::inputBands> patch{};
            for (float& v : patch)
                v = dist(rng);
            return patch;
        }
    } // namespace

    static void BM_MusicNNModel_forward(benchmark::State& state)
    {
        const std::filesystem::path path{ getMusicNNModelPathFromEnv() };
        if (path.empty())
        {
            state.SkipWithMessage("LMS_MUSICNN_MODEL not set");
            return;
        }

        const MusicNNModel model{ path };
        const auto patch{ makeRandomPatch() };

        for (auto _ : state)
            benchmark::DoNotOptimize(model.forward(patch));
    }

    BENCHMARK(BM_MusicNNModel_forward);

} // namespace lms::audio::musicnn::benchmarks
