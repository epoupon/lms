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

            const std::size_t requiredSampleCount{ frameCount * _hopSize };
            if (!readAtLeastSamples(requiredSampleCount))
                return 0;

            consumeSamples(requiredSampleCount);
            _currentFrameIndex += frameCount;
            return frameCount;
        }

        [[nodiscard]] std::size_t currentFrameIndex() const noexcept { return _currentFrameIndex; }

        std::size_t totalDecodedSamples() const noexcept { return _totalDecodedSamples; }

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
                _totalDecodedSamples += samplesRead;
            }

            return _bufferedSampleCount >= sampleCount;
        }

        void consumeSamples(std::size_t samplesToDrop)
        {
            assert(samplesToDrop <= _bufferedSampleCount);

            if (samplesToDrop < _bufferedSampleCount)
            {
                std::move(_samplesBuffer.begin() + static_cast<std::ptrdiff_t>(samplesToDrop),
                          _samplesBuffer.begin() + static_cast<std::ptrdiff_t>(_bufferedSampleCount),
                          _samplesBuffer.begin());
                _bufferedSampleCount -= samplesToDrop;
            }
            else
            {
                _bufferedSampleCount = 0;
            }
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
        std::size_t _totalDecodedSamples{};
        std::size_t _currentFrameIndex{};
        bool _endOfStream{};
    };
} // namespace lms::audio
