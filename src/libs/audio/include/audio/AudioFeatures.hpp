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

#pragma once

#include <array>
#include <iosfwd>
#include <span>

namespace lms::audio
{
    using FeatureValue = float;

    struct AudioFeatures
    {
        // log mel energies
        static inline constexpr size_t melBandCount{ 40 };
        std::array<FeatureValue, melBandCount> logMelEnergyMean;
        std::array<FeatureValue, melBandCount> logMelEnergyStdDev;
        std::array<FeatureValue, melBandCount> logMelEnergyDeltaStdDev;
        std::array<FeatureValue, melBandCount> logMelEnergyDeltaMeanAbs;

        // MFCCs
        static inline constexpr size_t mfccCount{ 13 };
        std::array<FeatureValue, mfccCount> mfccMean;
        std::array<FeatureValue, mfccCount> mfccStdDev;
        std::array<FeatureValue, mfccCount> mfccDeltaStdDev;
        std::array<FeatureValue, mfccCount> mfccDeltaMeanAbs;

        // Spectral features
        FeatureValue spectralCentroidMean;
        FeatureValue spectralCentroidStdDev;
        FeatureValue spectralCentroidDeltaMeanAbs;
        FeatureValue spectralCentroidDeltaStdDev;
        FeatureValue spectralRolloffMean;
        FeatureValue spectralRolloffStdDev;
        FeatureValue spectralRolloffDeltaMeanAbs;
        FeatureValue spectralRolloffDeltaStdDev;
        FeatureValue spectralFluxMean;
        FeatureValue spectralFluxStdDev;
        FeatureValue spectralFluxDeltaMeanAbs;

        // Chroma (pitch class / harmonic content)
        static inline constexpr size_t chromaCount{ 12 };
        std::array<FeatureValue, chromaCount> chromaMean;
        std::array<FeatureValue, chromaCount> chromaStdDev;
        std::array<FeatureValue, chromaCount> chromaDeltaStdDev;
        std::array<FeatureValue, chromaCount> chromaDeltaMeanAbs;

        // Zero crossing rate
        FeatureValue zeroCrossingRateMean;
        FeatureValue zeroCrossingRateStdDev;
    };

    // Buffer size must be at least sizeof(AudioFeatures)
    void audioFeaturesToBlob(const AudioFeatures& features, std::span<std::byte> buffer);
    void audioFeaturesFromBlob(std::span<const std::byte> buffer, AudioFeatures& features);

    std::ostream& operator<<(std::ostream& os, const AudioFeatures& features);
} // namespace lms::audio