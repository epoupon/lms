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
#include <span>

namespace lms::audio
{
    using FeatureValue = float;

    struct AudioFeatures
    {
        static inline constexpr size_t melBandCount{ 20 };
        // log mel energies
        std::array<FeatureValue, melBandCount> logMelEnergyMean;
        std::array<FeatureValue, melBandCount> logMelEnergyStdDev;
        std::array<FeatureValue, melBandCount> logMelEnergySkewness;

        // delta log mel energies
        std::array<FeatureValue, melBandCount> logMelEnergyDeltaStdDev;
    };

    // Buffer size must be at least sizeof(AudioFeatures)
    void audioFeaturesToBlob(const AudioFeatures& features, std::span<std::byte> buffer);
    void audioFeaturesFromBlob(std::span<const std::byte> buffer, AudioFeatures& features);
} // namespace lms::audio