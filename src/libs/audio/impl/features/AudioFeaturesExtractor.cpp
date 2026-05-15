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

#include "AudioFeaturesExtractor.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

#include "core/ILogger.hpp"

#include "math/StatsAccumulator.hpp"

#include "OnsetCalculator.hpp"
#include "SpectralFeatureCalculator.hpp"
#include "ZeroCrossingRateCalculator.hpp"

namespace lms::audio
{
    std::unique_ptr<IAudioFeaturesExtractor> createAudioFeaturesExtractor()
    {
        return std::make_unique<features::AudioFeaturesExtractor>();
    }
} // namespace lms::audio

namespace lms::audio::features
{
    namespace detail
    {
        constexpr AudioFeaturesExtractor::FloatType minMeaningfulPatchMeanRms{ 0.003F };

        template<typename FloatType>
        [[nodiscard]] FloatType computeFrameRms(std::span<const FloatType> samples)
        {
            const FloatType squaredMean{ std::transform_reduce(samples.begin(), samples.end(), FloatType{}, std::plus<>{}, [](const FloatType sample) {
                                             return sample * sample;
                                         })
                                         / static_cast<FloatType>(samples.size()) };

            return std::sqrt(squaredMean);
        }

        struct PatchFeatureStatsAccumulators
        {
            explicit PatchFeatureStatsAccumulators(std::size_t sampleOffset)
                : startSample{ sampleOffset }
                , melAccumulators{ AudioFeatures::melBandCount }
                , mfccAccumulators{ AudioFeatures::mfccCount }
                , chromaAccumulators{ AudioFeatures::chromaCount }
            {
            }

            void addFrame(const AudioFeatures& frameFeatures, AudioFeaturesExtractor::FloatType frameRms)
            {
                for (std::size_t m{}; m < AudioFeatures::melBandCount; ++m)
                    melAccumulators[m].add(frameFeatures.logMel[m]);

                for (std::size_t k{}; k < AudioFeatures::mfccCount; ++k)
                    mfccAccumulators[k].add(frameFeatures.mfcc[k]);

                spectralCentroidAccumulator.add(frameFeatures.spectralCentroid);

                spectralRolloffAccumulator.add(frameFeatures.spectralRolloff);

                spectralFluxAccumulator.add(frameFeatures.spectralFlux);

                onsetStrengthAccumulator.add(frameFeatures.onsetStrength);

                for (std::size_t c{}; c < AudioFeatures::chromaCount; ++c)
                    chromaAccumulators[c].add(frameFeatures.chroma[c]);

                zeroCrossingRateAccumulator.add(frameFeatures.zeroCrossingRate);
                frameRmsAccumulator.add(frameRms);
            }

            [[nodiscard]] bool hasFrames() const
            {
                return frameRmsAccumulator.getCount() > 0;
            }

            [[nodiscard]] bool isMeaningful() const
            {
                return frameRmsAccumulator.getMean() >= minMeaningfulPatchMeanRms;
            }

            [[nodiscard]] AudioFeaturesPatchStats finalize() const
            {
                AudioFeaturesPatchStats features{};

                for (std::size_t m{}; m < AudioFeatures::melBandCount; ++m)
                {
                    features.logMelMean[m] = melAccumulators[m].getMean();
                    features.logMelStdDev[m] = melAccumulators[m].getSampleStdDev();
                }

                for (std::size_t k{}; k < AudioFeatures::mfccCount; ++k)
                {
                    features.mfccMean[k] = mfccAccumulators[k].getMean();
                    features.mfccStdDev[k] = mfccAccumulators[k].getSampleStdDev();
                }

                features.spectralCentroidMean = spectralCentroidAccumulator.getMean();
                features.spectralCentroidStdDev = spectralCentroidAccumulator.getSampleStdDev();
                features.spectralRolloffMean = spectralRolloffAccumulator.getMean();
                features.spectralRolloffStdDev = spectralRolloffAccumulator.getSampleStdDev();
                features.spectralFluxMean = spectralFluxAccumulator.getMean();
                features.spectralFluxStdDev = spectralFluxAccumulator.getSampleStdDev();
                features.onsetStrengthMean = onsetStrengthAccumulator.getMean();
                features.onsetStrengthStdDev = onsetStrengthAccumulator.getSampleStdDev();

                for (std::size_t c{}; c < AudioFeatures::chromaCount; ++c)
                {
                    features.chromaMean[c] = chromaAccumulators[c].getMean();
                    features.chromaStdDev[c] = chromaAccumulators[c].getSampleStdDev();
                }

                features.zeroCrossingRateMean = zeroCrossingRateAccumulator.getMean();
                features.zeroCrossingRateStdDev = zeroCrossingRateAccumulator.getSampleStdDev();

                return features;
            }

            std::size_t startSample{};

            std::vector<math::StatsAccumulator<AudioFeaturesExtractor::FloatType>> melAccumulators;
            std::vector<math::StatsAccumulator<AudioFeaturesExtractor::FloatType>> mfccAccumulators;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> spectralCentroidAccumulator;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> spectralRolloffAccumulator;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> spectralFluxAccumulator;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> onsetStrengthAccumulator;
            std::vector<math::StatsAccumulator<AudioFeaturesExtractor::FloatType>> chromaAccumulators;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> zeroCrossingRateAccumulator;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> frameRmsAccumulator;
        };

        struct TrackMeanStatsAccumulators
        {
            TrackMeanStatsAccumulators()
                : melMeanAccumulators{ AudioFeatures::melBandCount }
                , mfccMeanAccumulators{ AudioFeatures::mfccCount }
                , chromaMeanAccumulators{ AudioFeatures::chromaCount }
            {
            }

            void add(const AudioFeaturesPatchStats& patch)
            {
                for (std::size_t m{}; m < AudioFeatures::melBandCount; ++m)
                {
                    melMeanAccumulators[m].add(patch.logMelMean[m]);
                }

                for (std::size_t k{}; k < AudioFeatures::mfccCount; ++k)
                {
                    mfccMeanAccumulators[k].add(patch.mfccMean[k]);
                }

                spectralCentroidMeanAccumulator.add(patch.spectralCentroidMean);

                spectralRolloffMeanAccumulator.add(patch.spectralRolloffMean);

                spectralFluxMeanAccumulator.add(patch.spectralFluxMean);

                onsetStrengthMeanAccumulator.add(patch.onsetStrengthMean);

                for (std::size_t c{}; c < AudioFeatures::chromaCount; ++c)
                {
                    chromaMeanAccumulators[c].add(patch.chromaMean[c]);
                }

                zeroCrossingRateMeanAccumulator.add(patch.zeroCrossingRateMean);
            }

            [[nodiscard]] TrackAudioFeatures finalize() const
            {
                TrackAudioFeatures trackFeatures{};

                for (std::size_t m{}; m < AudioFeatures::melBandCount; ++m)
                {
                    trackFeatures.mean.logMelMean[m] = melMeanAccumulators[m].getMean();
                    trackFeatures.mean.logMelStdDev[m] = 0.F;
                }

                for (std::size_t k{}; k < AudioFeatures::mfccCount; ++k)
                {
                    trackFeatures.mean.mfccMean[k] = mfccMeanAccumulators[k].getMean();
                    trackFeatures.mean.mfccStdDev[k] = 0.F;
                }

                trackFeatures.mean.spectralCentroidMean = spectralCentroidMeanAccumulator.getMean();
                trackFeatures.mean.spectralCentroidStdDev = 0.F;

                trackFeatures.mean.spectralRolloffMean = spectralRolloffMeanAccumulator.getMean();
                trackFeatures.mean.spectralRolloffStdDev = 0.F;

                trackFeatures.mean.spectralFluxMean = spectralFluxMeanAccumulator.getMean();
                trackFeatures.mean.spectralFluxStdDev = 0.F;

                trackFeatures.mean.onsetStrengthMean = onsetStrengthMeanAccumulator.getMean();
                trackFeatures.mean.onsetStrengthStdDev = 0.F;

                for (std::size_t c{}; c < AudioFeatures::chromaCount; ++c)
                {
                    trackFeatures.mean.chromaMean[c] = chromaMeanAccumulators[c].getMean();
                    trackFeatures.mean.chromaStdDev[c] = 0.F;
                }

                trackFeatures.mean.zeroCrossingRateMean = zeroCrossingRateMeanAccumulator.getMean();
                trackFeatures.mean.zeroCrossingRateStdDev = 0.F;

                return trackFeatures;
            }

            std::vector<math::StatsAccumulator<AudioFeaturesExtractor::FloatType>> melMeanAccumulators;
            std::vector<math::StatsAccumulator<AudioFeaturesExtractor::FloatType>> mfccMeanAccumulators;

            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> spectralCentroidMeanAccumulator;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> spectralRolloffMeanAccumulator;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> spectralFluxMeanAccumulator;
            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> onsetStrengthMeanAccumulator;

            std::vector<math::StatsAccumulator<AudioFeaturesExtractor::FloatType>> chromaMeanAccumulators;

            math::StatsAccumulator<AudioFeaturesExtractor::FloatType> zeroCrossingRateMeanAccumulator;
        };
    } // namespace detail

    AudioFeaturesExtractor::AudioFeaturesExtractor()
        : _melFilterBank{ computeMelFilterBank(_frameSize, _sampleRate, AudioFeatures::melBandCount) }
        , _mfccCalculator{}
        , _chromaCalculator{ static_cast<FloatType>(_sampleRate) }
    {
        assert(getSampleSize(PcmSampleType::Float32) == sizeof(FeatureValue));

        LMS_LOG(AUDIO, DEBUG, "Frame size = " << _frameSize << " samples (" << helpers::sampleCountToDuration<std::chrono::milliseconds>(static_cast<std::size_t>(_frameSize), _sampleRate).count() << " ms)");
    }

    AudioFeaturesExtractor::~AudioFeaturesExtractor() = default;

    AudioFeaturesExtractor::FeatureExtractionResult AudioFeaturesExtractor::extractFeatures(const std::filesystem::path& audioFile) const
    {
        FeatureExtractionResult res;
        res.metadata.frameSize = _frameSize;
        res.metadata.frameHopSize = _frameSize / 2;
        res.metadata.patchSize = (_sampleRate + res.metadata.frameHopSize - 1) / res.metadata.frameHopSize; // ~1s patches in frames
        res.metadata.patchHopSize = res.metadata.patchSize;
        res.metadata.pcmSampleRate = _sampleRate;

        FrameDecoder frameDecoder{ audioFile,
                                   PcmParameters{ .channelCount = 1,
                                                  .sampleRate = _sampleRate,
                                                  .sampleType = PcmSampleType::Float32,
                                                  .byteOrder = std::endian::native,
                                                  .planar = false },
                                   res.metadata.frameHopSize };

        assert(res.metadata.patchHopSize >= res.metadata.patchSize);

        // per-frame values feeding patch accumulators
        std::array<FloatType, AudioFeatures::melBandCount> logMel{};
        std::array<FloatType, AudioFeatures::melBandCount> melEnergies{};
        std::array<FloatType, AudioFeatures::mfccCount> mfccValues{};

        SpectralFeatureCalculator<FrameDecoder::spectrumSize, FloatType> spectralFeaturesCalculator;
        const FloatType binWidthHz{ FloatType(_sampleRate) / (2 * FrameDecoder::spectrumSize) };
        std::array<FloatType, AudioFeatures::chromaCount> normalizedChroma{};

        ZeroCrossingRateCalculator<FloatType> zeroCrossingRateCalculator;
        OnsetCalculator<AudioFeatures::melBandCount, FloatType> onsetCalculator;

        detail::TrackMeanStatsAccumulators trackMeanStatsAccumulators;

        const std::size_t patchFrameCount{ res.metadata.patchSize };
        const std::size_t patchHopFrameCount{ res.metadata.patchHopSize };
        assert(patchHopFrameCount >= patchFrameCount);

        std::size_t frameIndex{};
        while (true)
        {
            detail::PatchFeatureStatsAccumulators patchStats{ frameIndex * res.metadata.frameHopSize };

            const auto onFrame{ [&](const FrameDecoder::SpectralFrameView& frame) {
                for (std::size_t m{}; m < _melFilterBank.getFilterCount(); ++m)
                {
                    const float melEnergy{ _melFilterBank.computeEnergy(m, std::span<const FloatType>(frame.powerSpectrum)) };
                    melEnergies[m] = melEnergy;
                    logMel[m] = std::logf(melEnergy + 1e-10F); // add small constant to avoid log(0)
                }

                const FloatType onsetStrength{ onsetCalculator.apply(melEnergies) };

                const auto computedMfccValues{ _mfccCalculator.apply(logMel) };
                std::copy(computedMfccValues.cbegin(), computedMfccValues.cend(), mfccValues.begin());

                const auto spectralFeatures{ spectralFeaturesCalculator.apply(frame.powerSpectrum, binWidthHz) };

                const auto computedChroma{ _chromaCalculator.apply(frame.powerSpectrum) };
                std::copy(computedChroma.cbegin(), computedChroma.cend(), normalizedChroma.begin());

                const FloatType zeroCrossingRate{ zeroCrossingRateCalculator.apply(std::span<const FloatType>(frame.rawSamples)) };
                const FloatType frameRms{ detail::computeFrameRms(std::span<const FloatType>(frame.rawSamples)) };

                AudioFeatures frameAudioFeatures{};
                std::copy(logMel.cbegin(), logMel.cend(), frameAudioFeatures.logMel.begin());
                std::copy(mfccValues.cbegin(), mfccValues.cend(), frameAudioFeatures.mfcc.begin());
                frameAudioFeatures.spectralCentroid = spectralFeatures.spectralCentroid;
                frameAudioFeatures.spectralRolloff = spectralFeatures.spectralRolloff;
                frameAudioFeatures.spectralFlux = spectralFeatures.spectralFlux;
                frameAudioFeatures.onsetStrength = onsetStrength;
                std::copy(normalizedChroma.cbegin(), normalizedChroma.cend(), frameAudioFeatures.chroma.begin());
                frameAudioFeatures.zeroCrossingRate = zeroCrossingRate;

                patchStats.addFrame(frameAudioFeatures, frameRms);

                ++frameIndex;
                ++res.metadata.frameCount;
            } };

            if (frameDecoder.decodeFrames(patchFrameCount, onFrame) < patchFrameCount)
                break;

            if (patchStats.hasFrames() && patchStats.isMeaningful())
            {
                trackMeanStatsAccumulators.add(patchStats.finalize());
                res.metadata.patchCount++;
            }

            const std::size_t framesToSkip{ patchHopFrameCount - patchFrameCount };
            if (framesToSkip > 0)
            {
                if (frameDecoder.skipFrames(framesToSkip) == 0)
                    break;
                frameIndex += framesToSkip;
            }
        }
        res.metadata.pcmSampleCount = frameDecoder.totalDecodedSamples();

        res.features = trackMeanStatsAccumulators.finalize();

        return res;
    }
} // namespace lms::audio::features