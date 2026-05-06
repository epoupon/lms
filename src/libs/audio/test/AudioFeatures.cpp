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
    TEST(AudioFeatures, blobConversion)
    {
        AudioFeatures features;

        std::iota(features.logMelEnergyMean.begin(), features.logMelEnergyMean.end(), 0.F);
        std::iota(features.logMelEnergyStdDev.begin(), features.logMelEnergyStdDev.end(), 10.F);
        std::iota(features.logMelEnergyDeltaStdDev.begin(), features.logMelEnergyDeltaStdDev.end(), 30.F);
        std::iota(features.logMelEnergyDeltaMeanAbs.begin(), features.logMelEnergyDeltaMeanAbs.end(), 31.F);

        std::iota(features.mfccMean.begin(), features.mfccMean.end(), 40.F);
        std::iota(features.mfccStdDev.begin(), features.mfccStdDev.end(), 50.F);
        std::iota(features.mfccDeltaStdDev.begin(), features.mfccDeltaStdDev.end(), 60.F);
        std::iota(features.mfccDeltaMeanAbs.begin(), features.mfccDeltaMeanAbs.end(), 61.F);

        features.spectralCentroidMean = 70.F;
        features.spectralCentroidStdDev = 71.F;
        features.spectralCentroidDeltaMeanAbs = 72.F;
        features.spectralCentroidDeltaStdDev = 73.F;
        features.spectralRolloffMean = 74.F;
        features.spectralRolloffStdDev = 75.F;
        features.spectralRolloffDeltaMeanAbs = 76.F;
        features.spectralRolloffDeltaStdDev = 77.F;
        features.spectralFluxMean = 78.F;
        features.spectralFluxStdDev = 79.F;
        features.spectralFluxDeltaMeanAbs = 80.F;

        std::iota(features.chromaMean.begin(), features.chromaMean.end(), 81.F);
        std::iota(features.chromaStdDev.begin(), features.chromaStdDev.end(), 88.F);
        std::iota(features.chromaDeltaStdDev.begin(), features.chromaDeltaStdDev.end(), 100.F);
        std::iota(features.chromaDeltaMeanAbs.begin(), features.chromaDeltaMeanAbs.end(), 112.F);

        features.zeroCrossingRateMean = 124.F;
        features.zeroCrossingRateStdDev = 113.F;

        std::vector<std::byte> buffer(sizeof(AudioFeatures));
        audioFeaturesToBlob(features, buffer);

        AudioFeatures readBackFeatures;
        audioFeaturesFromBlob(buffer, readBackFeatures);

        for (std::size_t i{}; i < features.logMelEnergyMean.size(); ++i)
            EXPECT_FLOAT_EQ(features.logMelEnergyMean[i], readBackFeatures.logMelEnergyMean[i]);

        for (std::size_t i{}; i < features.logMelEnergyStdDev.size(); ++i)
            EXPECT_FLOAT_EQ(features.logMelEnergyStdDev[i], readBackFeatures.logMelEnergyStdDev[i]);

        for (std::size_t i{}; i < features.logMelEnergyDeltaStdDev.size(); ++i)
            EXPECT_FLOAT_EQ(features.logMelEnergyDeltaStdDev[i], readBackFeatures.logMelEnergyDeltaStdDev[i]);

        for (std::size_t i{}; i < features.logMelEnergyDeltaMeanAbs.size(); ++i)
            EXPECT_FLOAT_EQ(features.logMelEnergyDeltaMeanAbs[i], readBackFeatures.logMelEnergyDeltaMeanAbs[i]);

        for (std::size_t i{}; i < features.mfccMean.size(); ++i)
            EXPECT_FLOAT_EQ(features.mfccMean[i], readBackFeatures.mfccMean[i]);

        for (std::size_t i{}; i < features.mfccStdDev.size(); ++i)
            EXPECT_FLOAT_EQ(features.mfccStdDev[i], readBackFeatures.mfccStdDev[i]);

        for (std::size_t i{}; i < features.mfccDeltaStdDev.size(); ++i)
            EXPECT_FLOAT_EQ(features.mfccDeltaStdDev[i], readBackFeatures.mfccDeltaStdDev[i]);

        for (std::size_t i{}; i < features.mfccDeltaMeanAbs.size(); ++i)
            EXPECT_FLOAT_EQ(features.mfccDeltaMeanAbs[i], readBackFeatures.mfccDeltaMeanAbs[i]);

        EXPECT_FLOAT_EQ(features.spectralCentroidMean, readBackFeatures.spectralCentroidMean);
        EXPECT_FLOAT_EQ(features.spectralCentroidStdDev, readBackFeatures.spectralCentroidStdDev);
        EXPECT_FLOAT_EQ(features.spectralCentroidDeltaMeanAbs, readBackFeatures.spectralCentroidDeltaMeanAbs);
        EXPECT_FLOAT_EQ(features.spectralCentroidDeltaStdDev, readBackFeatures.spectralCentroidDeltaStdDev);
        EXPECT_FLOAT_EQ(features.spectralRolloffMean, readBackFeatures.spectralRolloffMean);
        EXPECT_FLOAT_EQ(features.spectralRolloffStdDev, readBackFeatures.spectralRolloffStdDev);
        EXPECT_FLOAT_EQ(features.spectralRolloffDeltaMeanAbs, readBackFeatures.spectralRolloffDeltaMeanAbs);
        EXPECT_FLOAT_EQ(features.spectralRolloffDeltaStdDev, readBackFeatures.spectralRolloffDeltaStdDev);
        EXPECT_FLOAT_EQ(features.spectralFluxMean, readBackFeatures.spectralFluxMean);
        EXPECT_FLOAT_EQ(features.spectralFluxStdDev, readBackFeatures.spectralFluxStdDev);
        EXPECT_FLOAT_EQ(features.spectralFluxDeltaMeanAbs, readBackFeatures.spectralFluxDeltaMeanAbs);

        for (std::size_t i{}; i < features.chromaMean.size(); ++i)
            EXPECT_FLOAT_EQ(features.chromaMean[i], readBackFeatures.chromaMean[i]);

        for (std::size_t i{}; i < features.chromaStdDev.size(); ++i)
            EXPECT_FLOAT_EQ(features.chromaStdDev[i], readBackFeatures.chromaStdDev[i]);

        for (std::size_t i{}; i < features.chromaDeltaStdDev.size(); ++i)
            EXPECT_FLOAT_EQ(features.chromaDeltaStdDev[i], readBackFeatures.chromaDeltaStdDev[i]);

        for (std::size_t i{}; i < features.chromaDeltaMeanAbs.size(); ++i)
            EXPECT_FLOAT_EQ(features.chromaDeltaMeanAbs[i], readBackFeatures.chromaDeltaMeanAbs[i]);

        EXPECT_FLOAT_EQ(features.zeroCrossingRateMean, readBackFeatures.zeroCrossingRateMean);
        EXPECT_FLOAT_EQ(features.zeroCrossingRateStdDev, readBackFeatures.zeroCrossingRateStdDev);
    }
} // namespace lms::audio::features::tests