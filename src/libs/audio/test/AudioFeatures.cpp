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

#include <numeric>

#include <gtest/gtest.h>

#include "audio/AudioFeatures.hpp"

namespace lms::audio::features::tests
{
    TEST(AudioFeatures, trackBlobConversion)
    {
        TrackAudioFeatures features{};

        std::iota(features.mean.logMelMean.begin(), features.mean.logMelMean.end(), 0.F);
        std::iota(features.mean.logMelStdDev.begin(), features.mean.logMelStdDev.end(), 10.F);

        std::iota(features.mean.mfccMean.begin(), features.mean.mfccMean.end(), 40.F);
        std::iota(features.mean.mfccStdDev.begin(), features.mean.mfccStdDev.end(), 50.F);

        features.mean.spectralCentroidMean = 60.F;
        features.mean.spectralCentroidStdDev = 61.F;
        features.mean.zeroCrossingRateMean = 62.F;
        features.mean.zeroCrossingRateStdDev = 63.F;

        std::vector<std::byte> buffer(sizeof(TrackAudioFeatures));
        trackAudioFeaturesToBlob(features, buffer);

        TrackAudioFeatures readBackFeatures{};
        trackAudioFeaturesFromBlob(buffer, readBackFeatures);

        EXPECT_FLOAT_EQ(features.mean.logMelMean[0], readBackFeatures.mean.logMelMean[0]);
        EXPECT_FLOAT_EQ(features.mean.logMelStdDev[0], readBackFeatures.mean.logMelStdDev[0]);
        EXPECT_FLOAT_EQ(features.mean.mfccMean[0], readBackFeatures.mean.mfccMean[0]);
        EXPECT_FLOAT_EQ(features.mean.mfccStdDev[0], readBackFeatures.mean.mfccStdDev[0]);
        EXPECT_FLOAT_EQ(features.mean.spectralCentroidMean, readBackFeatures.mean.spectralCentroidMean);
        EXPECT_FLOAT_EQ(features.mean.spectralCentroidStdDev, readBackFeatures.mean.spectralCentroidStdDev);
        EXPECT_FLOAT_EQ(features.mean.zeroCrossingRateMean, readBackFeatures.mean.zeroCrossingRateMean);
        EXPECT_FLOAT_EQ(features.mean.zeroCrossingRateStdDev, readBackFeatures.mean.zeroCrossingRateStdDev);
    }
} // namespace lms::audio::features::tests