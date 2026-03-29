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

#include "audio/IAudioFeaturesExtractor.hpp"
#include "core/ILogger.hpp"

#include "audio/IPcmDecoder.hpp"

#include "DeltaCalculator.hpp"
#include "IFFT.hpp"
#include "StatsAccumulator.hpp"
#include "Window.hpp"

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
        std::vector<AudioFeaturesExtractor::FloatType> computeWindow(std::size_t frameSize)
        {
            std::vector<AudioFeaturesExtractor::FloatType> window;
            window.resize(frameSize);
            computeHannWindow(window);
            return window;
        }
    } // namespace detail

    AudioFeaturesExtractor::AudioFeaturesExtractor()
        : _pcmParams{ .channelCount = 1, .sampleRate = 22050, .sampleType = PcmSampleType::Float32, .byteOrder = std::endian::native, .planar = false }
        , _frameSize{ 1024 }
        , _window{ detail::computeWindow(_frameSize) }
        , _windowEnergy{ static_cast<float>(std::accumulate(_window.begin(), _window.end(), double{}, [](double sum, double w) { return sum + w * w; })) }
        , _melFilterBank{ computeMelFilterBank(_frameSize, _pcmParams.sampleRate, AudioFeatures::melBandCount) }
    {
        assert(_window.size() == _frameSize);
        assert(getSampleSize(_pcmParams.sampleType) == sizeof(FeatureValueType));

        LMS_LOG(AUDIO, DEBUG, "Frame size = " << _frameSize << " samples (" << helpers::sampleCountToDuration<std::chrono::milliseconds>(static_cast<std::size_t>(_frameSize), _pcmParams.sampleRate).count() << " ms)");
    }

    AudioFeaturesExtractor::~AudioFeaturesExtractor() = default;

    AudioFeatures AudioFeaturesExtractor::process(const std::filesystem::path& audioFile) const
    {
        auto pcmDecoder{ createPcmDecoder(audioFile, {}, _pcmParams) };

        std::vector<FloatType> samplesBuffer;
        constexpr std::size_t samplesBufferFrameCount{ 10 };
        samplesBuffer.resize(samplesBufferFrameCount * _frameSize);

        AudioFeatures features;
        features.frameSize = _frameSize;
        features.frameHopSize = _frameSize / 2;
        features.pcmSampleRate = _pcmParams.sampleRate;

        auto fft{ createRealFFTPlan(_frameSize) };
        std::span<FloatType> windowedFrame{ fft->getInputBuffer() };
        std::vector<FloatType> powerSpectrum(fft->getOutputBuffer().size());
        const FloatType powerScale{ 1.F / (_windowEnergy * _frameSize) };
        std::vector<StatsAccumulator> logMelEnergyAccumulators(_melFilterBank.getFilterCount());
        std::vector<StatsAccumulator> logMelEnergyDeltaAccumulators(_melFilterBank.getFilterCount());
        std::vector<DeltaCalculator> logMelEnergyDeltaCalculators(_melFilterBank.getFilterCount(), DeltaCalculator{ 5 });

        std::size_t currentSampleOffset{}; // in samples
        while (true)
        {
            const std::size_t decodedSampleCount{ readSamples(*pcmDecoder, std::span<FloatType>(samplesBuffer).subspan(currentSampleOffset)) };
            if (decodedSampleCount == 0) // EOF reached
            {
                assert(currentSampleOffset < _frameSize);
                break; // ignore what is left in the buffer, as it is not a full frame
            }

            features.pcmSampleCount += decodedSampleCount;

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

                fft->apply();

                // compute power spectrum
                std::transform(fft->getOutputBuffer().begin(), fft->getOutputBuffer().end(), powerSpectrum.begin(), [powerScale](const std::complex<FloatType>& bin) {
                    return (bin.real() * bin.real() + bin.imag() * bin.imag()) * powerScale;
                });

                // compute log-Mel energies and accumulate stats
                for (std::size_t m{}; m < _melFilterBank.getFilterCount(); ++m)
                {
                    const float melEnergy{ _melFilterBank.computeEnergy(m, powerSpectrum) };
                    const float logMelEnergy{ std::logf(melEnergy + 1e-10F) }; // add small constant to avoid log(0)
                    logMelEnergyAccumulators[m].add(logMelEnergy);

                    if (const std::optional<FloatType> melEnergyDelta{ logMelEnergyDeltaCalculators[m].add(logMelEnergy) })
                        logMelEnergyDeltaAccumulators[m].add(*melEnergyDelta);
                }

                features.frameCount++;

                currentSampleOffset += _frameSize / 2;
            }

            const std::size_t remainingSampleCount{ availableSampleCount - currentSampleOffset };

            // technically should not overlap since buffer is large enough
            std::move_backward(availableSamples.begin() + currentSampleOffset, availableSamples.end(), samplesBuffer.begin() + remainingSampleCount);
            currentSampleOffset = remainingSampleCount;
        }

        for (std::size_t m{}; m < _melFilterBank.getFilterCount(); ++m)
        {
            features.logMelEnergies[m].mean = logMelEnergyAccumulators[m].getMean();
            features.logMelEnergies[m].stddev = logMelEnergyAccumulators[m].getStdDev();
            features.logMelEnergies[m].skewness = logMelEnergyAccumulators[m].getSkewness();

            features.logMelDeltaEnergies[m].mean = logMelEnergyDeltaAccumulators[m].getMean();
            features.logMelDeltaEnergies[m].stddev = logMelEnergyDeltaAccumulators[m].getStdDev();
            features.logMelDeltaEnergies[m].skewness = logMelEnergyDeltaAccumulators[m].getSkewness();
        }

        return features;
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