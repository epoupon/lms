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

#include "audio/IPcmDecoder.hpp"

#include "FFmpegTypes.hpp"

namespace lms::audio::ffmpeg
{
    class PcmDecoder : public IPcmDecoder
    {
    public:
        PcmDecoder(const std::filesystem::path& filePath, std::chrono::microseconds offset, const PcmParameters& parameters);
        ~PcmDecoder() override;

        PcmDecoder(const PcmDecoder&) = delete;
        PcmDecoder& operator=(const PcmDecoder&) = delete;

    private:
        const PcmParameters& getParameters() const override;

        std::size_t readSamples(std::span<WritableBuffer> outputChannelBuffers) override;
        bool finished() const override;

        std::chrono::milliseconds getEstimatedDuration() const override;

        std::size_t computeSampleCountPerChannel(std::span<WritableBuffer> outputChannelBuffers) const;
        void feedDecoder();
        std::size_t drainResampler(std::span<WritableBuffer> outputChannelBuffers, std::size_t maxSamplesPerChannel);
        std::size_t getEstimatedResamplerAvailableSamples() const;

        const PcmParameters _parameters;

        bool _finished{};
        bool _eof{};
        bool _draining{};

        AVFormatContextPtr _context;
        std::chrono::milliseconds _estimatedDuration{};
        int _inputStreamIndex{};
        AVCodecContextPtr _decoderContext;
        AVFramePtr _decodedFrame;
        AVPacketPtr _inputPacket;
        SwrContextPtr _resampleContext;
    };
} // namespace lms::audio::ffmpeg