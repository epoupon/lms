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

#include "audio/Exception.hpp"
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

        // We want something like this:
        // gap patch(0) gap patch(1) gap patch(maxPatchCount) gap
        std::size_t computePatchGap(std::size_t totalFrameCount, std::size_t patchFrameCount, std::size_t maxPatchCount)
        {
            assert(maxPatchCount > 0);
            assert(patchFrameCount > 0);

            const std::size_t patchCount{ std::min(maxPatchCount, totalFrameCount / patchFrameCount) };
            if (patchCount == 0)
                return 0;

            return (totalFrameCount - patchCount * patchFrameCount) / (patchCount + 1);
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

        void reset() noexcept
        {
            _frameCount = {};
            _rmsAccum = {};
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
        std::array<float, patchFrameCount * melBandCount> _melMatrix;
    };

    MusicNNEmbeddingExtractor::MusicNNEmbeddingExtractor(const std::filesystem::path& modelPath, std::size_t maxPatchCount)
        : _melFilterBank{ features::computeMelFilterBank(fftSize, sampleRate, melBandCount, melFMin, melFMax) }
        , _model{ modelPath }
        , _maxPatchCount{ maxPatchCount }
    {
        static_assert(MusicNNEmbeddingExtractor::windowSize == MusicNNEmbeddingExtractor::fftSize);
        if (_maxPatchCount <= 0)
            throw audio::Exception{ "MusicNN embedding extractor: max patch count must be > 0" };
    }

    IMusicNNEmbeddingExtractor::ExtractionResult MusicNNEmbeddingExtractor::extract(const std::filesystem::path& audioFile) const
    {
        auto frameDecoder{ std::make_unique<FrameDecoder>(audioFile,
                                                          PcmParameters{ .channelCount = 1,
                                                                         .sampleRate = static_cast<unsigned>(sampleRate),
                                                                         .sampleType = PcmSampleType::Float32,
                                                                         .byteOrder = std::endian::native,
                                                                         .planar = false },
                                                          frameHopSamples) };

        std::array<float, melBandCount> logMelRow{};
        std::array<math::StatsAccumulator<float>, decltype(_model)::outputSize> embeddingAccumulators;
        ExtractionResult result;
        const std::size_t estimatedFrameCount{ frameDecoder->getEstimatedFrameCount() };

        // Fallback: use a gap of two patch lengths if the frame count is unknown
        const std::size_t patchGapFrameCount{ estimatedFrameCount ? computePatchGap(frameDecoder->getEstimatedFrameCount(), patchFrameCount, _maxPatchCount) : (2 * patchFrameCount) };
        const auto patchAccumulator{ std::make_unique<PatchAccumulator>() };

        while (true)
        {
            patchAccumulator->reset();

            const auto onFrame{ [&](const FrameDecoder::SpectralFrameView& frame) {
                // MusicNN log compression: log10(10000 * mel + 1)
                for (std::size_t m{}; m < melBandCount; ++m)
                {
                    const float energy{ _melFilterBank.computeEnergy(m, std::span<const float>(frame.powerSpectrum)) };
                    logMelRow[m] = std::log10(10000.F * energy + 1.F);
                }

                const float rms{ computeRms(frame.rawSamples.subspan(0, frameHopSamples)) };
                patchAccumulator->addMelRow(logMelRow, rms);
            } };

            if (patchGapFrameCount > 0 && frameDecoder->skipFrames(patchGapFrameCount) == 0)
                break;

            if (frameDecoder->decodeFrames(patchFrameCount, onFrame) < patchFrameCount)
                break;

            assert(patchAccumulator->complete());

            if (!patchAccumulator->meaningful())
                continue;

            const auto embedding{ _model.forward(patchAccumulator->data()) };
            for (std::size_t d{}; d < embedding.size(); ++d)
                embeddingAccumulators[d].add(embedding[d]);
            ++result.patchCount;
        }

        if (result.patchCount > 0)
        {
            for (std::size_t d{}; d < decltype(_model)::outputSize; ++d)
                result.embeddings.mean.values[d] = embeddingAccumulators[d].getMean();
        }

        return result;
    }
} // namespace lms::audio::musicnn
