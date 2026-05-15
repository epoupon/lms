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
        // Log mel energies
        static inline constexpr size_t melBandCount{ 40 };
        std::array<FeatureValue, melBandCount> logMel;

        // MFCCs
        static inline constexpr size_t mfccCount{ 13 };
        std::array<FeatureValue, mfccCount> mfcc;

        // Spectral features
        FeatureValue spectralCentroid;
        FeatureValue spectralRolloff;
        FeatureValue spectralFlux;

        // Onset strength (mel-band flux novelty)
        FeatureValue onsetStrength;

        // Chroma (pitch class / harmonic content)
        static inline constexpr size_t chromaCount{ 12 };
        std::array<FeatureValue, chromaCount> chroma;

        // Zero crossing rate
        FeatureValue zeroCrossingRate;
    };

    struct AudioFeaturesPatchStats
    {
        std::array<FeatureValue, AudioFeatures::melBandCount> logMelMean;
        std::array<FeatureValue, AudioFeatures::melBandCount> logMelStdDev;

        std::array<FeatureValue, AudioFeatures::mfccCount> mfccMean;
        std::array<FeatureValue, AudioFeatures::mfccCount> mfccStdDev;

        FeatureValue spectralCentroidMean;
        FeatureValue spectralCentroidStdDev;
        FeatureValue spectralRolloffMean;
        FeatureValue spectralRolloffStdDev;
        FeatureValue spectralFluxMean;
        FeatureValue spectralFluxStdDev;

        FeatureValue onsetStrengthMean;
        FeatureValue onsetStrengthStdDev;

        std::array<FeatureValue, AudioFeatures::chromaCount> chromaMean;
        std::array<FeatureValue, AudioFeatures::chromaCount> chromaStdDev;

        FeatureValue zeroCrossingRateMean;
        FeatureValue zeroCrossingRateStdDev;
    };

    struct TrackAudioFeatures
    {
        AudioFeaturesPatchStats mean;
    };

    // Buffer size must be at least sizeof(TrackAudioFeatures)
    void trackAudioFeaturesToBlob(const TrackAudioFeatures& features, std::span<std::byte> buffer);
    void trackAudioFeaturesFromBlob(std::span<const std::byte> buffer, TrackAudioFeatures& features);

    std::ostream& operator<<(std::ostream& os, const AudioFeatures& features);
    std::ostream& operator<<(std::ostream& os, const AudioFeaturesPatchStats& features);
    std::ostream& operator<<(std::ostream& os, const TrackAudioFeatures& features);
} // namespace lms::audio