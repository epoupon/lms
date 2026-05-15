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

#include "audio/IAudioFeaturesExtractor.hpp"

#include "ChromaCalculator.hpp"
#include "MelFilterBank.hpp"
#include "MfccCalculator.hpp"
#include "utils/PcmSpectralFrameDecoder.hpp"

namespace lms::audio::features
{
    class AudioFeaturesExtractor : public IAudioFeaturesExtractor
    {
    public:
        AudioFeaturesExtractor();
        ~AudioFeaturesExtractor() override;
        AudioFeaturesExtractor(const AudioFeaturesExtractor&) = delete;
        AudioFeaturesExtractor& operator=(const AudioFeaturesExtractor&) = delete;

        using FloatType = FeatureValue;

    private:
        [[nodiscard]] FeatureExtractionResult extractFeatures(const std::filesystem::path& audioFile) const override;

        static constexpr std::size_t _sampleRate{ 16000 };
        static constexpr std::size_t _frameSize{ 512 };
        using FrameDecoder = PcmSpectralFrameDecoder<_frameSize, FloatType>;
        const MelFilterBank _melFilterBank;
        const MfccCalculator<AudioFeatures::melBandCount, AudioFeatures::mfccCount, float> _mfccCalculator;
        const ChromaCalculator<FrameDecoder::spectrumSize> _chromaCalculator;
    };
} // namespace lms::audio::features