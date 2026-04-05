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

#pragma once

#include <filesystem>
#include <memory>

#include "audio/AudioFeatures.hpp"

namespace lms::audio
{
    class IAudioFeaturesExtractor
    {
    public:
        virtual ~IAudioFeaturesExtractor() = default;

        struct AnalysisMetadata
        {
            std::size_t frameSize{};
            std::size_t frameHopSize{};
            std::size_t pcmSampleRate{};
            std::size_t pcmSampleCount{}; // total number of processed PCM samples
            std::size_t frameCount{};     // total number of processed frames (with overlapping)
        };

        struct FeatureExtractionResult
        {
            AudioFeatures features;
            AnalysisMetadata metadata;
        };
        [[nodiscard]] virtual FeatureExtractionResult extractFeatures(const std::filesystem::path& audioFile) const = 0;
    };

    std::unique_ptr<IAudioFeaturesExtractor> createAudioFeaturesExtractor();
} // namespace lms::audio