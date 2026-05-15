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

#include "MusicNNEmbeddingExtractor.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numeric>

#include "audio/IMusicNNEmbeddingExtractor.hpp"
#include "math/StatsAccumulator.hpp"
#include "musicnn/MusicNNModel.hpp"

namespace lms::audio::musicnn
{
    namespace
    {
        constexpr float minMeaningfulPatchRms{ 0.003F };

        template<typename FloatType>
        FloatType computeRms(std::span<const FloatType> samples)
        {
            const FloatType sumSq{ std::transform_reduce(samples.begin(), samples.end(), FloatType{}, std::plus<>{}, [](FloatType s) { return s * s; }) };
            return std::sqrt(sumSq / static_cast<FloatType>(samples.size()));
        }

    } // namespace

    // Accumulates one 187-frame MusicNN mel patch
    class MusicNNEmbeddingExtractor::PatchAccumulator
    {
    public:
        void addMelRow(std::span<const float, melBandCount> melRow, float frameRms)
        {
            assert(_frameCount < patchFrameCount);
            const std::size_t offset{ _frameCount * melBandCount };
            std::copy(melRow.begin(), melRow.end(), _melMatrix.begin() + static_cast<std::ptrdiff_t>(offset));
            _rmsAccum += frameRms;
            ++_frameCount;
        }

        [[nodiscard]] bool complete() const { return _frameCount == patchFrameCount; }

        [[nodiscard]] bool meaningful() const
        {
            return (_frameCount > 0) && ((_rmsAccum / static_cast<float>(_frameCount)) >= minMeaningfulPatchRms);
        }

        [[nodiscard]] std::span<const float, patchFrameCount * melBandCount> data() const
        {
            return _melMatrix;
        }

    private:
        std::size_t _frameCount{};
        float _rmsAccum{};
        std::array<float, patchFrameCount * melBandCount> _melMatrix{};
    };

    MusicNNEmbeddingExtractor::MusicNNEmbeddingExtractor(const std::filesystem::path& modelPath)
        : _melFilterBank{ features::computeMelFilterBank(fftSize, sampleRate, melBandCount, melFMin, melFMax) }
        , _model{ modelPath }
    {
        static_assert(MusicNNEmbeddingExtractor::windowSize == MusicNNEmbeddingExtractor::fftSize);
    }

    IMusicNNEmbeddingExtractor::ExtractionResult MusicNNEmbeddingExtractor::extract(const std::filesystem::path& audioFile) const
    {
        FrameDecoder frameDecoder{ audioFile,
                                   PcmParameters{ .channelCount = 1,
                                                  .sampleRate = static_cast<unsigned>(sampleRate),
                                                  .sampleType = PcmSampleType::Float32,
                                                  .byteOrder = std::endian::native,
                                                  .planar = false },
                                   frameHop };

        std::array<float, melBandCount> logMelRow{};
        std::array<math::StatsAccumulator<float>, decltype(_model)::outputSize> embeddingAccumulators;
        ExtractionResult result;

        while (true)
        {
            PatchAccumulator patch;

            const auto onFrame{ [&](const FrameDecoder::SpectralFrameView& frame) {
                // MusicNN log compression: log10(10000 * mel + 1)
                for (std::size_t m{}; m < melBandCount; ++m)
                {
                    const float energy{ _melFilterBank.computeEnergy(m, std::span<const float>(frame.powerSpectrum)) };
                    logMelRow[m] = std::log10(10000.F * energy + 1.F);
                }

                const float rms{ computeRms(frame.rawSamples.subspan(0, frameHop)) };
                patch.addMelRow(logMelRow, rms);
            } };

            if (frameDecoder.decodeFrames(patchFrameCount, onFrame) < patchFrameCount)
                break;

            assert(patch.complete());

            if (!patch.meaningful())
                continue;

            const auto embedding{ _model.forward(patch.data()) };
            for (std::size_t d{}; d < embedding.size(); ++d)
                embeddingAccumulators[d].add(embedding[d]);
            ++result.patchCount;

            static_assert(patchHopFrames >= patchFrameCount);
            const std::size_t framesToSkip{ patchHopFrames - patchFrameCount };
            if (framesToSkip > 0 && frameDecoder.skipFrames(framesToSkip) == 0)
                break;
        }

        if (result.patchCount > 0)
        {
            for (std::size_t d{}; d < decltype(_model)::outputSize; ++d)
                result.embeddings.mean.values[d] = embeddingAccumulators[d].getMean();
        }

        return result;
    }
} // namespace lms::audio::musicnn
