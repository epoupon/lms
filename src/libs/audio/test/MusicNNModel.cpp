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

#include <gtest/gtest.h>

#include "musicnn/MusicNNModel.hpp"

namespace lms::audio::musicnn::tests
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

    TEST(MusicNNModel, CanConstruct)
    {
        const std::filesystem::path path{ getMusicNNModelPathFromEnv() };

        if (path.empty())
            GTEST_SKIP() << "LMS_MUSICNN_MODEL not set";

        EXPECT_NO_THROW({ const MusicNNModel model{ path }; });
    }

    TEST(MusicNNModel, CanForward)
    {
        const std::filesystem::path path{ getMusicNNModelPathFromEnv() };

        if (path.empty())
            GTEST_SKIP() << "LMS_MUSICNN_MODEL not set";

        const MusicNNModel model{ path };
        const auto patch{ makeRandomPatch() };

        EXPECT_NO_THROW({ [[maybe_unused]] const auto output{ model.forward(patch) }; });
    }

} // namespace lms::audio::musicnn::tests