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

#include "audio/IAudioFeaturesExtractor.hpp"
#include "audio/PcmTypes.hpp"

#include "MelFilterBank.hpp"

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

        using FloatType = FeatureValueType;

    private:
        AudioFeatures process(const std::filesystem::path& audioFile) const override;

        std::size_t readSamples(IPcmDecoder& pcmDecoder, std::span<FloatType> buffer) const;

        const PcmParameters _pcmParams;
        const std::size_t _frameSize;
        const std::vector<FloatType> _window;
        const float _windowEnergy;
        const MelFilterBank _melFilterBank;
    };
} // namespace lms::audio::features