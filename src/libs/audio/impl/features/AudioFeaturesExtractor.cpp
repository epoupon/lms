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
#include <cmath>
#include <numeric>

#include "core/AlignedHeapArray.hpp"
#include "core/ILogger.hpp"

#include "audio/IPcmDecoder.hpp"
#include "math/StatsAccumulator.hpp"
#include "math/Window.hpp"

#include "DeltaCalculator.hpp"
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
        template<std::size_t Size, typename FloatType>
        std::array<FloatType, Size> computeWindow()
        {
            std::array<FloatType, Size> window;
            math::computeHannWindow<AudioFeaturesExtractor::FloatType>(window);
            return window;
        }
    } // namespace detail

    AudioFeaturesExtractor::AudioFeaturesExtractor()
        : _pcmParams{ .channelCount = 1, .sampleRate = 22050, .sampleType = PcmSampleType::Float32, .byteOrder = std::endian::native, .planar = false }
        , _window{ detail::computeWindow<_frameSize, FloatType>() }
        , _windowEnergy{ static_cast<float>(std::accumulate(_window.begin(), _window.end(), double{}, [](double sum, double w) { return sum + w * w; })) }
        , _melFilterBank{ computeMelFilterBank(_frameSize, _pcmParams.sampleRate, AudioFeatures::melBandCount) }
        , _mfccCalculator{}
        , _chromaCalculator{ static_cast<FloatType>(_pcmParams.sampleRate) }
    {
        assert(_window.size() == _frameSize);
        assert(getSampleSize(_pcmParams.sampleType) == sizeof(FeatureValue));

        LMS_LOG(AUDIO, DEBUG, "Frame size = " << _frameSize << " samples (" << helpers::sampleCountToDuration<std::chrono::milliseconds>(static_cast<std::size_t>(_frameSize), _pcmParams.sampleRate).count() << " ms)");
    }

    AudioFeaturesExtractor::~AudioFeaturesExtractor() = default;

    AudioFeaturesExtractor::FeatureExtractionResult AudioFeaturesExtractor::extractFeatures(const std::filesystem::path& audioFile) const
    {
        auto pcmDecoder{ createPcmDecoder(audioFile, {}, _pcmParams) };

        std::vector<FloatType> samplesBuffer;
        constexpr std::size_t samplesBufferFrameCount{ 10 };
        samplesBuffer.resize(samplesBufferFrameCount * _frameSize);

        FeatureExtractionResult res;
        res.metadata.frameSize = _frameSize;
        res.metadata.frameHopSize = _frameSize / 2;
        res.metadata.pcmSampleRate = _pcmParams.sampleRate;

        core::AlignedHeapArray<FloatType, FFTPlan::minBufferAlignment> windowedFrame{ FFTPlan::getInputSize() };
        core::AlignedHeapArray<std::complex<FloatType>, FFTPlan::minBufferAlignment> fftOutput{ FFTPlan::getOutputSize() };
        std::array<FloatType, FFTPlan::getOutputSize()> powerSpectrum{};
        const FloatType powerScale{ 1.F / (_windowEnergy * _frameSize) };

        // log mel energy
        std::vector<math::StatsAccumulator<FloatType>> logMelEnergyAccumulators(_melFilterBank.getFilterCount());
        std::vector<math::StatsAccumulator<FloatType>> logMelEnergyDeltaAccumulators(_melFilterBank.getFilterCount());
        std::vector<math::StatsAccumulator<FloatType>> logMelEnergyDeltaMeanAbsAccumulators(_melFilterBank.getFilterCount());
        std::vector<DeltaCalculator> logMelEnergyDeltaCalculators(_melFilterBank.getFilterCount(), DeltaCalculator{ 5 });
        std::array<FloatType, AudioFeatures::melBandCount> logMel{};

        // MFCCs
        std::vector<math::StatsAccumulator<FloatType>> mfccAccumulators(AudioFeatures::mfccCount);
        std::vector<math::StatsAccumulator<FloatType>> mfccDeltaAccumulators(AudioFeatures::mfccCount);
        std::vector<math::StatsAccumulator<FloatType>> mfccDeltaMeanAbsAccumulators(AudioFeatures::mfccCount);
        std::vector<DeltaCalculator> mfccDeltaCalculators(AudioFeatures::mfccCount, DeltaCalculator{ 5 });

        // Spectral features
        SpectralFeatureCalculator<powerSpectrum.size(), FloatType> spectralFeaturesCalculator;
        const FloatType binWidthHz{ FloatType(_pcmParams.sampleRate) / (2 * powerSpectrum.size()) };
        // - centroid
        math::StatsAccumulator<FloatType> spectralCentroidAccumulator;
        DeltaCalculator spectralCentroidDeltaCalculator{ 5 };
        math::StatsAccumulator<FloatType> spectralCentroidDeltaAccumulator;
        // - rolloff
        math::StatsAccumulator<FloatType> spectralRolloffAccumulator;
        DeltaCalculator spectralRolloffDeltaCalculator{ 5 };
        math::StatsAccumulator<FloatType> spectralRolloffDeltaAccumulator;
        // - flux
        math::StatsAccumulator<FloatType> spectralFluxAccumulator;
        DeltaCalculator spectralFluxDeltaCalculator{ 5 };
        math::StatsAccumulator<FloatType> spectralFluxDeltaAccumulator;

        // Chroma
        std::vector<math::StatsAccumulator<FloatType>> chromaAccumulators(AudioFeatures::chromaCount);
        std::vector<math::StatsAccumulator<FloatType>> chromaDeltaAccumulators(AudioFeatures::chromaCount);
        std::vector<math::StatsAccumulator<FloatType>> chromaDeltaMeanAbsAccumulators(AudioFeatures::chromaCount);
        std::vector<DeltaCalculator> chromaDeltaCalculators(AudioFeatures::chromaCount, DeltaCalculator{ 5 });

        // Zero crossing rate
        math::StatsAccumulator<FloatType> zeroCrossingRateAccumulator;
        ZeroCrossingRateCalculator<FloatType> zeroCrossingRateCalculator;

        std::size_t currentSampleOffset{}; // in samples
        while (true)
        {
            const std::size_t decodedSampleCount{ readSamples(*pcmDecoder, std::span<FloatType>(samplesBuffer).subspan(currentSampleOffset)) };
            if (decodedSampleCount == 0) // EOF reached
            {
                assert(currentSampleOffset < _frameSize);
                break; // ignore what is left in the buffer, as it is not a full frame
            }

            res.metadata.pcmSampleCount += decodedSampleCount;

            const std::size_t availableSampleCount{ currentSampleOffset + decodedSampleCount };
            std::span<FloatType> availableSamples{ samplesBuffer.data(), availableSampleCount };

            currentSampleOffset = 0;
            while (currentSampleOffset + _frameSize <= availableSamples.size())
            {
                std::span<FloatType> samples{ availableSamples.subspan(currentSampleOffset, _frameSize) };
                assert(samples.size() == _frameSize);

                // apply window
                for (std::size_t i{}; i < _frameSize; ++i)
                    windowedFrame[i] = samples[i] * _window[i];

                _realFFTPlan.apply(windowedFrame, fftOutput);

                // compute power spectrum
                std::transform(fftOutput.cbegin(), fftOutput.cend(), powerSpectrum.begin(), [powerScale](const std::complex<FloatType>& bin) {
                    return (bin.real() * bin.real() + bin.imag() * bin.imag()) * powerScale;
                });

                // compute log-Mel energies and accumulate stats
                for (std::size_t m{}; m < _melFilterBank.getFilterCount(); ++m)
                {
                    const float melEnergy{ _melFilterBank.computeEnergy(m, powerSpectrum) };
                    const float logMelEnergy{ std::logf(melEnergy + 1e-10F) }; // add small constant to avoid log(0)
                    logMel[m] = logMelEnergy;
                    logMelEnergyAccumulators[m].add(logMelEnergy);

                    if (const std::optional<FloatType> melEnergyDelta{ logMelEnergyDeltaCalculators[m].add(logMelEnergy) })
                    {
                        logMelEnergyDeltaAccumulators[m].add(*melEnergyDelta);
                        logMelEnergyDeltaMeanAbsAccumulators[m].add(std::abs(*melEnergyDelta));
                    }
                }

                const auto mfccValues{ _mfccCalculator.apply(logMel) };
                for (std::size_t k{}; k < AudioFeatures::mfccCount; ++k)
                {
                    mfccAccumulators[k].add(mfccValues[k]);

                    if (const std::optional<FloatType> mfccDelta{ mfccDeltaCalculators[k].add(mfccValues[k]) })
                    {
                        mfccDeltaAccumulators[k].add(*mfccDelta);
                        mfccDeltaMeanAbsAccumulators[k].add(std::abs(*mfccDelta));
                    }
                }

                {
                    const auto spectralFeatures{ spectralFeaturesCalculator.apply(powerSpectrum, binWidthHz) };

                    spectralCentroidAccumulator.add(spectralFeatures.spectralCentroid);
                    if (const std::optional<FloatType> centroidDelta{ spectralCentroidDeltaCalculator.add(spectralFeatures.spectralCentroid) })
                        spectralCentroidDeltaAccumulator.add(std::abs(*centroidDelta));

                    spectralRolloffAccumulator.add(spectralFeatures.spectralRolloff);
                    if (const std::optional<FloatType> rolloffDelta{ spectralRolloffDeltaCalculator.add(spectralFeatures.spectralRolloff) })
                        spectralRolloffDeltaAccumulator.add(std::abs(*rolloffDelta));

                    spectralFluxAccumulator.add(spectralFeatures.spectralFlux);
                    if (const std::optional<FloatType> fluxDelta{ spectralFluxDeltaCalculator.add(spectralFeatures.spectralFlux) })
                        spectralFluxDeltaAccumulator.add(std::abs(*fluxDelta));
                }

                // Chroma
                {
                    const auto normalizedChroma{ _chromaCalculator.apply(powerSpectrum) };

                    for (std::size_t c{}; c < AudioFeatures::chromaCount; ++c)
                    {
                        chromaAccumulators[c].add(normalizedChroma[c]);

                        if (const auto d = chromaDeltaCalculators[c].add(normalizedChroma[c]))
                        {
                            chromaDeltaAccumulators[c].add(*d);
                            chromaDeltaMeanAbsAccumulators[c].add(std::abs(*d));
                        }
                    }
                }

                // Zero crossing rate
                {
                    const FloatType zcr{ zeroCrossingRateCalculator.apply(samples) };
                    zeroCrossingRateAccumulator.add(zcr);
                }

                res.metadata.frameCount++;

                currentSampleOffset += _frameSize / 2;
            }

            const std::size_t remainingSampleCount{ availableSampleCount - currentSampleOffset };

            // technically should not overlap since buffer is large enough
            std::move_backward(availableSamples.begin() + currentSampleOffset, availableSamples.end(), samplesBuffer.begin() + remainingSampleCount);
            currentSampleOffset = remainingSampleCount;
        }

        for (std::size_t m{}; m < _melFilterBank.getFilterCount(); ++m)
        {
            res.features.logMelEnergyMean[m] = logMelEnergyAccumulators[m].getMean();
            res.features.logMelEnergyStdDev[m] = logMelEnergyAccumulators[m].getSampleStdDev();
            res.features.logMelEnergyDeltaStdDev[m] = logMelEnergyDeltaAccumulators[m].getSampleStdDev();
            res.features.logMelEnergyDeltaMeanAbs[m] = logMelEnergyDeltaMeanAbsAccumulators[m].getMean();
        }

        for (std::size_t k{}; k < AudioFeatures::mfccCount; ++k)
        {
            res.features.mfccMean[k] = mfccAccumulators[k].getMean();
            res.features.mfccStdDev[k] = mfccAccumulators[k].getSampleStdDev();
            res.features.mfccDeltaStdDev[k] = mfccDeltaAccumulators[k].getSampleStdDev();
            res.features.mfccDeltaMeanAbs[k] = mfccDeltaMeanAbsAccumulators[k].getMean();
        }

        res.features.spectralCentroidMean = spectralCentroidAccumulator.getMean();
        res.features.spectralCentroidStdDev = spectralCentroidAccumulator.getSampleStdDev();
        res.features.spectralCentroidDeltaMeanAbs = spectralCentroidDeltaAccumulator.getMean();
        res.features.spectralCentroidDeltaStdDev = spectralCentroidDeltaAccumulator.getSampleStdDev();
        res.features.spectralRolloffMean = spectralRolloffAccumulator.getMean();
        res.features.spectralRolloffStdDev = spectralRolloffAccumulator.getSampleStdDev();
        res.features.spectralRolloffDeltaMeanAbs = spectralRolloffDeltaAccumulator.getMean();
        res.features.spectralRolloffDeltaStdDev = spectralRolloffDeltaAccumulator.getSampleStdDev();
        res.features.spectralFluxMean = spectralFluxAccumulator.getMean();
        res.features.spectralFluxStdDev = spectralFluxAccumulator.getSampleStdDev();
        res.features.spectralFluxDeltaMeanAbs = spectralFluxDeltaAccumulator.getMean();

        for (std::size_t c{}; c < AudioFeatures::chromaCount; ++c)
        {
            res.features.chromaMean[c] = chromaAccumulators[c].getMean();
            res.features.chromaStdDev[c] = chromaAccumulators[c].getSampleStdDev();
            res.features.chromaDeltaStdDev[c] = chromaDeltaAccumulators[c].getSampleStdDev();
            res.features.chromaDeltaMeanAbs[c] = chromaDeltaMeanAbsAccumulators[c].getMean();
        }

        res.features.zeroCrossingRateMean = zeroCrossingRateAccumulator.getMean();
        res.features.zeroCrossingRateStdDev = zeroCrossingRateAccumulator.getSampleStdDev();

        return res;
    }

    std::size_t AudioFeaturesExtractor::readSamples(IPcmDecoder& pcmDecoder, std::span<FloatType> buffer) const
    {
        std::size_t totalSampleCount{};

        while (!buffer.empty())
        {
            std::array outputBuffers{ audio::IPcmDecoder::WritableBuffer{ std::as_writable_bytes(buffer) } };
            const std::size_t sampleCount{ pcmDecoder.readSamples(outputBuffers) };
            if (sampleCount == 0)
                break;

            buffer = buffer.subspan(sampleCount);
            totalSampleCount += sampleCount;
        }

        return totalSampleCount;
    }
} // namespace lms::audio::features