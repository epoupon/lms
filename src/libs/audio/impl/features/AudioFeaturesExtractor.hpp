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
#include "audio/PcmTypes.hpp"
#include "math/FFT.hpp"

#include "ChromaCalculator.hpp"
#include "MelFilterBank.hpp"
#include "MfccCalculator.hpp"

namespace lms::audio
{
    class IPcmDecoder;
}

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

        std::size_t readSamples(IPcmDecoder& pcmDecoder, std::span<FloatType> buffer) const;

        const PcmParameters _pcmParams;
        static constexpr std::size_t _frameSize{ 1024 };
        const std::array<FloatType, _frameSize> _window;
        const float _windowEnergy;
        const MelFilterBank _melFilterBank;
        using FFTPlan = math::FixedRealFFTPlan<_frameSize>;
        const FFTPlan _realFFTPlan;
        const MfccCalculator<AudioFeatures::melBandCount, AudioFeatures::mfccCount, float> _mfccCalculator;
        const ChromaCalculator<FFTPlan::getOutputSize()> _chromaCalculator;
    };
} // namespace lms::audio::features