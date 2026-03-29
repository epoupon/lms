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
#include <numeric>

#include <gtest/gtest.h>

#include "features/MelFilterBank.hpp"

namespace lms::audio::features::tests
{
    constexpr float epsilon{ 1e-5F };

    constexpr std::size_t NFFT{ 2048 };
    constexpr std::size_t sampleRate{ 22050 };
    constexpr std::size_t filterCount{ 40 };

    TEST(MelFilterBank, sizeCheck)
    {
        const MelFilterBank bank{ computeMelFilterBank(NFFT, sampleRate, filterCount) };
        EXPECT_EQ(bank.getFilterCount(), filterCount);
        EXPECT_EQ(bank.getBinCount(), NFFT / 2 + 1);
    }

    TEST(MelFilterBank, nonNegativeWeights)
    {
        const MelFilterBank bank{ computeMelFilterBank(NFFT, sampleRate, filterCount) };

        for (std::size_t m{}; m < bank.getFilterCount(); ++m)
        {
            const auto& filter = bank.getFilter(m);
            for (float w : filter.weights)
                EXPECT_GE(w, 0.F) << "Filter " << m << " has negative weight: " << w;
        }
    }

    TEST(MelFilterBank, peaksAreNonZero)
    {
        const MelFilterBank bank{ computeMelFilterBank(NFFT, sampleRate, filterCount) };

        for (std::size_t m{}; m < bank.getFilterCount(); ++m)
        {
            const auto& filter{ bank.getFilter(m) };
            const float maxVal{ *std::max_element(filter.weights.begin(), filter.weights.end()) };

            EXPECT_GT(maxVal, 0.F) << "Filter " << m << " has zero peak value";
        }
    }

    TEST(MelFilterBank, filtersAreUnitSum)
    {
        const MelFilterBank bank{ computeMelFilterBank(NFFT, sampleRate, filterCount) };

        for (std::size_t m{}; m < bank.getFilterCount(); ++m)
        {
            const auto& filter{ bank.getFilter(m) };
            const float sum{ std::accumulate(filter.weights.begin(), filter.weights.end(), 0.F) };

            EXPECT_NEAR(sum, 1.F, epsilon) << "Filter " << m << " sum = " << sum;
        }
    }

    TEST(MelFilterBank, everyBinCovered)
    {
        const MelFilterBank bank{ computeMelFilterBank(NFFT, sampleRate, filterCount) };
        const std::size_t binCount{ bank.getBinCount() };

        std::vector<bool> covered(binCount, false);

        for (std::size_t m{}; m < bank.getFilterCount(); ++m)
        {
            const auto& filter{ bank.getFilter(m) };

            std::size_t bin{ filter.leftBinIndex };
            for (float w : filter.weights)
            {
                if (w > 0.F)
                    covered[bin] = true;

                ++bin;
            }
        }

        // Find actual covered range
        auto first{ std::find(covered.begin(), covered.end(), true) };
        auto last{ std::find(covered.rbegin(), covered.rend(), true).base() };

        ASSERT_NE(first, covered.end()); // sanity

        for (auto it = first; it != last; ++it)
        {
            EXPECT_TRUE(*it) << "A bin in the covered range is not covered by any filter";
        }
    }

    TEST(MelFilterBank, overlapAtMostTwo)
    {
        const MelFilterBank bank{ computeMelFilterBank(NFFT, sampleRate, filterCount) };
        const std::size_t binCount{ bank.getBinCount() };

        std::vector<std::size_t> overlap(binCount, 0);

        for (std::size_t m{}; m < bank.getFilterCount(); ++m)
        {
            const auto& filter{ bank.getFilter(m) };

            std::size_t bin{ filter.leftBinIndex };
            for (float w : filter.weights)
            {
                if (w > 0.F)
                    overlap[bin]++;
                ++bin;
            }
        }

        for (std::size_t k{}; k < binCount; ++k)
        {
            EXPECT_LE(overlap[k], 2) << "Bin " << k << " is covered by " << overlap[k] << " filters";
        }
    }

    TEST(MelFilterBank, flatSpectrumEnergySanity)
    {
        const MelFilterBank bank{ computeMelFilterBank(NFFT, sampleRate, filterCount) };
        std::vector<float> flatSpectrum(bank.getBinCount(), 1.F);

        for (std::size_t m{}; m < bank.getFilterCount(); ++m)
        {
            const float energy{ bank.computeEnergy(m, flatSpectrum) };
            EXPECT_GT(energy, 0.F) << "Filter " << m << " has zero energy for flat spectrum";
        }
    }
} // namespace lms::audio::features::tests