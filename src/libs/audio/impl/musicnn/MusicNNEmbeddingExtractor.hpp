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

#include "audio/IMusicNNEmbeddingExtractor.hpp"

#include "MusicNNModel.hpp"
#include "features/MelFilterBank.hpp"
#include "utils/PcmSpectralFrameDecoder.hpp"

namespace lms::audio::musicnn
{
    class MusicNNEmbeddingExtractor : public IMusicNNEmbeddingExtractor
    {
    public:
        MusicNNEmbeddingExtractor(const std::filesystem::path& modelPath, std::size_t maxPatchCount);
        ~MusicNNEmbeddingExtractor() override = default;

        MusicNNEmbeddingExtractor(const MusicNNEmbeddingExtractor&) = delete;
        MusicNNEmbeddingExtractor& operator=(const MusicNNEmbeddingExtractor&) = delete;

    private:
        [[nodiscard]] ExtractionResult extract(const std::filesystem::path& audioFile) const override;

        // MusicNN signal processing constants (from musicnn/configuration.py and musicnn_torch.py)
        static constexpr std::size_t sampleRate{ 16'000 };
        static constexpr std::size_t windowSize{ 512 }; // 512-sample Hann window (32 ms)
        static constexpr std::size_t fftSize{ 512 };
        static constexpr std::size_t frameHopSamples{ 256 }; // 16 ms hop (matches FFT_HOP in musicnn)
        static constexpr std::size_t melBandCount{ 96 };
        static constexpr float melFMin{ 0.F };
        static constexpr float melFMax{ 8'000.F };
        static constexpr std::size_t patchFrameCount{ MusicNNModel::inputFrames }; // 187 frames = 3 s

        class PatchAccumulator;

        using FrameDecoder = PcmSpectralFrameDecoder<512, float>;
        const features::MelFilterBank _melFilterBank;
        const MusicNNModel _model;
        const std::size_t _maxPatchCount;
    };
} // namespace lms::audio::musicnn
