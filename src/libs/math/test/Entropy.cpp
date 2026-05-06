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

#include <gtest/gtest.h>

#include "math/Entropy.hpp"

namespace lms::math
{
    TEST(EntropyTest, ZeroInput)
    {
        std::array<float, 12> c{};

        const float e{ entropy<float>(c) };
        EXPECT_EQ(e, 0.f);
    }

    TEST(EntropyTest, SingleBinIsZeroEntropy)
    {
        std::array<float, 12> c{};
        c[3] = 1.F;

        const float e{ entropy<float>(c) };
        EXPECT_FLOAT_EQ(e, 0.F);
    }

    TEST(EntropyTest, UniformDistributionMaxEntropy)
    {
        std::array<float, 12> c;

        for (auto& v : c)
            v = 1.F;

        const float e{ entropy<float>(c) };
        const float expected{ std::log(12.f) };
        EXPECT_FLOAT_EQ(e, expected);
    }

    TEST(EntropyTest, ScaleInvariance)
    {
        std::array<float, 12> c{ 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f };

        const float a{ entropy<float>(c) };

        for (auto& v : c)
            v *= 1000.f;

        const float b{ entropy<float>(c) };

        EXPECT_FLOAT_EQ(a, b);
    }

    TEST(EntropyTest, MoreSpreadMeansHigherEntropy)
    {
        std::array<float, 12> tight{};
        std::array<float, 12> spread{};

        tight[5] = 0.5F;
        tight[6] = 0.5F;

        spread[2] = 0.3F;
        spread[6] = 0.4F;
        spread[9] = 0.3F;

        EXPECT_GT(entropy<float>(spread), entropy<float>(tight));
    }
} // namespace lms::math