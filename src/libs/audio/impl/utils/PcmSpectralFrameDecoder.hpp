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

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <complex>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include "core/AlignedHeapArray.hpp"

#include "audio/IPcmDecoder.hpp"
#include "audio/PcmTypes.hpp"
#include "math/FFT.hpp"
#include "math/Window.hpp"

namespace lms::audio
{
    // Stateful PCM frame decoder that applies a Hann window + FFT per frame.
    template<std::size_t WindowSize, typename FloatType = float>
    class PcmSpectralFrameDecoder
    {
        static_assert(std::has_single_bit(WindowSize), "WindowSize must be a power of two");

    public:
        using FFTPlan = math::FixedRealFFTPlan<WindowSize, FloatType>;
        static constexpr std::size_t spectrumSize{ FFTPlan::getOutputSize() };

        PcmSpectralFrameDecoder(const std::filesystem::path& audioFile, const PcmParameters& params, std::size_t hopSize)
            : PcmSpectralFrameDecoder{ createPcmDecoder(audioFile, {}, params), hopSize }
        {
        }
        ~PcmSpectralFrameDecoder() = default;

        explicit PcmSpectralFrameDecoder(std::unique_ptr<IPcmDecoder> decoder, std::size_t hopSize)
            : _pcmParams{ decoder->getParameters() }
            , _hopSize{ hopSize }
            , _powerScale{ FloatType{ 1 } / (_window.energy() * static_cast<FloatType>(WindowSize)) }
            , _decoder{ std::move(decoder) }
            , _samplesBuffer(bufferFrameCount * _hopSize + WindowSize)
            , _bufferedSampleCount{ WindowSize / 2 } // first analysis frame centered on sample 0, matching librosa center=True semantics.
        {
            assert(_hopSize > 0);
        }

        PcmSpectralFrameDecoder(const PcmSpectralFrameDecoder&) = delete;
        PcmSpectralFrameDecoder& operator=(const PcmSpectralFrameDecoder&) = delete;

        std::size_t hopSize() const noexcept { return _hopSize; }
        const PcmParameters& pcmParameters() const noexcept { return _pcmParams; }

        std::size_t getEstimatedFrameCount() const
        {
            const auto duration{ _decoder->getEstimatedDuration() };
            if (duration <= std::chrono::milliseconds::zero())
                return 0;

            const auto totalSamples{ static_cast<std::size_t>((static_cast<std::uint64_t>(duration.count()) * _pcmParams.sampleRate) / 1'000) };

            constexpr std::size_t halfWindow{ WindowSize / 2 };
            if (totalSamples < halfWindow) // Not enough samples to produce even the first frame.
                return 0;

            return 1 + ((totalSamples - halfWindow) / _hopSize);
        }

        // Spectral data for a single frame.
        struct SpectralFrameView
        {
            std::span<const FloatType, WindowSize> rawSamples;
            std::span<const FloatType, spectrumSize> powerSpectrum;
        };

        // Decodes up to frameCount frames, invoking callback for each. May return fewer than
        // frameCount at EOF. Returns 0 only if no frame at all could be decoded.
        template<typename Callback>
            requires std::invocable<Callback, const SpectralFrameView&>
        std::size_t decodeFrames(std::size_t frameCount, Callback&& callback)
        {
            if (frameCount == 0)
                return 0;

            std::size_t decodedCount{};
            while (decodedCount < frameCount)
            {
                if (!readAtLeastSamples(std::max(WindowSize, _hopSize)))
                    break;

                const std::span<const FloatType, WindowSize> rawSamples{ _samplesBuffer.data(), WindowSize };
                const std::span<FloatType, WindowSize> windowedFrame{ _windowedFrame.data(), WindowSize };
                _window.apply(rawSamples, windowedFrame);

                _fftPlan.apply(_windowedFrame, _fftOutput);

                // Reuse _windowedFrame for power spectrum (spectrumSize <= WindowSize).
                const std::span<FloatType, spectrumSize> powerBuffer{ _windowedFrame.data(), spectrumSize };
                std::transform(_fftOutput.cbegin(), _fftOutput.cend(), powerBuffer.begin(),
                               [this](const std::complex<FloatType>& bin) {
                                   return (bin.real() * bin.real() + bin.imag() * bin.imag()) * _powerScale;
                               });

                const SpectralFrameView frame{ .rawSamples = rawSamples,
                                               .powerSpectrum = std::span<const FloatType, spectrumSize>{ _windowedFrame.data(), spectrumSize } };
                std::invoke(callback, frame);

                consumeSamples(_hopSize);
                ++_currentFrameIndex;
                ++decodedCount;
            }

            return decodedCount;
        }

        // Skips exactly the next frameCount frames without computing FFT or invoking callbacks.
        // Returns frameCount on success, 0 on EOF.
        std::size_t skipFrames(std::size_t frameCount)
        {
            if (frameCount == 0)
                return 0;

            std::size_t skippedFrameCount{};
            while (skippedFrameCount < frameCount)
            {
                if (!readAtLeastSamples(std::max(WindowSize, _hopSize)))
                    break;

                consumeSamples(_hopSize);
                ++_currentFrameIndex;
                ++skippedFrameCount;
            }

            return skippedFrameCount;
        }

        [[nodiscard]] std::size_t currentFrameIndex() const noexcept { return _currentFrameIndex; }

    private:
        static constexpr std::size_t bufferFrameCount{ 20 };

        bool readAtLeastSamples(std::size_t sampleCount)
        {
            if (sampleCount <= _bufferedSampleCount)
                return true;

            if (_samplesBuffer.size() < sampleCount)
                _samplesBuffer.resize(sampleCount);

            while ((_bufferedSampleCount < sampleCount) && !_endOfStream)
            {
                std::span<FloatType> dest{ _samplesBuffer.data() + _bufferedSampleCount, _samplesBuffer.size() - _bufferedSampleCount };
                assert(!dest.empty());
                if (dest.empty())
                    break;

                std::array outputBuffers{ IPcmDecoder::WritableBuffer{ std::as_writable_bytes(dest) } };
                const std::size_t samplesRead{ _decoder->readSamples(outputBuffers) };
                if (samplesRead == 0)
                {
                    _endOfStream = true;
                    break;
                }

                _bufferedSampleCount += samplesRead;
            }

            return _bufferedSampleCount >= sampleCount;
        }

        void consumeSamples(std::size_t samplesToDrop)
        {
            // TODO use a circular buffer and only compacts at the end of the buffer
            assert(samplesToDrop <= _bufferedSampleCount);

            const auto remaining{ _bufferedSampleCount - samplesToDrop };
            if (remaining)
            {
                std::move(_samplesBuffer.begin() + samplesToDrop,
                          _samplesBuffer.begin() + _bufferedSampleCount,
                          _samplesBuffer.begin());
            }

            _bufferedSampleCount = remaining;
        }

        const PcmParameters _pcmParams;
        const std::size_t _hopSize;
        const math::HannWindow<WindowSize, FloatType> _window;
        const FloatType _powerScale;
        const FFTPlan _fftPlan{};
        std::unique_ptr<IPcmDecoder> _decoder;
        std::vector<FloatType> _samplesBuffer;
        core::AlignedHeapArray<FloatType, FFTPlan::minBufferAlignment> _windowedFrame{ FFTPlan::getInputSize() };
        core::AlignedHeapArray<std::complex<FloatType>, FFTPlan::minBufferAlignment> _fftOutput{ FFTPlan::getOutputSize() };
        std::size_t _bufferedSampleCount{};
        std::size_t _currentFrameIndex{};
        bool _endOfStream{};
    };
} // namespace lms::audio
