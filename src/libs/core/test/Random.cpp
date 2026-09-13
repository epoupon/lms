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

#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "core/Random.hpp"

namespace lms::core::tests
{
    TEST(Random, generateIntegralWithinRange)
    {
        for (int i{}; i < 100; ++i)
        {
            const int value{ random::generate(10, 20) };
            EXPECT_GE(value, 10);
            EXPECT_LE(value, 20);
        }
    }

    TEST(Random, generateFloatingPointWithinRange)
    {
        for (int i{}; i < 100; ++i)
        {
            const double value{ random::generate(1.5, 2.5) };
            EXPECT_GE(value, 1.5);
            EXPECT_LE(value, 2.5);
        }
    }

    TEST(Random, generateWithSameSeedIsDeterministic)
    {
        random::PseudoRandomGenerator gen1{ 1234 };
        random::PseudoRandomGenerator gen2{ 1234 };

        for (int i{}; i < 100; ++i)
        {
            EXPECT_EQ(random::generate(gen1, 0, 1'000'000), random::generate(gen2, 0, 1'000'000));
        }
    }

    TEST(Random, generateWithNonDeterministicGeneratorWithinRange)
    {
        random::NonDeterministicRandomGenerator rd;

        for (int i{}; i < 100; ++i)
        {
            const int value{ random::generate(rd, 10, 20) };
            EXPECT_GE(value, 10);
            EXPECT_LE(value, 20);
        }
    }

    TEST(Random, getPseudoRandomGeneratorReturnsStableInstance)
    {
        EXPECT_EQ(&random::getPseudoRandomGenerator(), &random::getPseudoRandomGenerator());
    }

    TEST(Random, getNonDeterministicRandomGeneratorReturnsStableInstance)
    {
        EXPECT_EQ(&random::getNonDeterministicRandomGenerator(), &random::getNonDeterministicRandomGenerator());
    }

    TEST(Random, fillContainerIntegral)
    {
        std::array<int, 50> values{};
        random::fillContainer(random::getPseudoRandomGenerator(), values, 5, 15);

        for (int value : values)
        {
            EXPECT_GE(value, 5);
            EXPECT_LE(value, 15);
        }
    }

    TEST(Random, fillContainerFloatingPoint)
    {
        std::array<double, 50> values{};
        random::fillContainer(random::getPseudoRandomGenerator(), values, -1.0, 1.0);

        for (double value : values)
        {
            EXPECT_GE(value, -1.0);
            EXPECT_LE(value, 1.0);
        }
    }

    TEST(Random, shuffleContainerPreservesElements)
    {
        std::vector<int> values(20);
        std::iota(values.begin(), values.end(), 0);
        const std::vector<int> original{ values };

        random::shuffleContainer(values);

        EXPECT_TRUE(std::is_permutation(values.begin(), values.end(), original.begin(), original.end()));
    }

    TEST(Random, shuffleContainerWithExplicitGeneratorPreservesElements)
    {
        std::vector<int> values(20);
        std::iota(values.begin(), values.end(), 0);
        const std::vector<int> original{ values };

        random::PseudoRandomGenerator gen{ 42 };
        random::shuffleContainer(gen, values);

        EXPECT_TRUE(std::is_permutation(values.begin(), values.end(), original.begin(), original.end()));
    }

    TEST(Random, pickRandomOnEmptyContainerReturnsEnd)
    {
        const std::vector<int> values;
        EXPECT_EQ(random::pickRandom(values), values.end());
    }

    TEST(Random, pickRandomReturnsElementFromContainer)
    {
        const std::vector<int> values{ 10, 20, 30, 40, 50 };

        for (int i{}; i < 100; ++i)
        {
            const auto it{ random::pickRandom(values) };
            ASSERT_NE(it, values.end());
            EXPECT_NE(std::find(values.begin(), values.end(), *it), values.end());
        }
    }
} // namespace lms::core::tests
