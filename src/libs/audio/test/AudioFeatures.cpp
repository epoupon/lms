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

#include <gtest/gtest.h>
#include <numeric>

#include "audio/AudioFeatures.hpp"

namespace lms::audio::features::tests
{
    TEST(AudioFeatures, blobConversion)
    {
        AudioFeatures features;

        std::iota(features.logMelEnergyMean.begin(), features.logMelEnergyMean.end(), 0.F);
        std::iota(features.logMelEnergyStdDev.begin(), features.logMelEnergyStdDev.end(), 10.F);
        std::iota(features.logMelEnergySkewness.begin(), features.logMelEnergySkewness.end(), 20.F);
        std::iota(features.logMelEnergyDeltaStdDev.begin(), features.logMelEnergyDeltaStdDev.end(), 30.F);

        std::vector<std::byte> buffer(sizeof(AudioFeatures));
        audioFeaturesToBlob(features, buffer);

        AudioFeatures readBackFeatures;
        audioFeaturesFromBlob(buffer, readBackFeatures);

        for (size_t i = 0; i < features.logMelEnergyMean.size(); ++i)
            EXPECT_FLOAT_EQ(features.logMelEnergyMean[i], readBackFeatures.logMelEnergyMean[i]);

        for (size_t i = 0; i < features.logMelEnergyStdDev.size(); ++i)
            EXPECT_FLOAT_EQ(features.logMelEnergyStdDev[i], readBackFeatures.logMelEnergyStdDev[i]);

        for (size_t i = 0; i < features.logMelEnergySkewness.size(); ++i)
            EXPECT_FLOAT_EQ(features.logMelEnergySkewness[i], readBackFeatures.logMelEnergySkewness[i]);

        for (size_t i = 0; i < features.logMelEnergyDeltaStdDev.size(); ++i)
            EXPECT_FLOAT_EQ(features.logMelEnergyDeltaStdDev[i], readBackFeatures.logMelEnergyDeltaStdDev[i]);
    }
} // namespace lms::audio::features::tests