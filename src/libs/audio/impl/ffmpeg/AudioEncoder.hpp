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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

#include "audio/PcmTypes.hpp"

#include "FFmpegTypes.hpp"

namespace lms::audio::ffmpeg
{
    struct EncodeParameters
    {
        core::media::Container container;
        core::media::Codec codec;
        std::optional<unsigned> bitrate;
        std::optional<unsigned> bitsPerSample;
        unsigned channelCount;
        unsigned sampleRate;
        std::unordered_map<std::string, std::string> metadata;
    };

    // Encodes PCM samples and muxes them into a non seekable byte stream
    class AudioEncoder
    {
    public:
        AudioEncoder(const EncodeParameters& parameters);
        ~AudioEncoder();

        AudioEncoder(const AudioEncoder&) = delete;
        AudioEncoder& operator=(const AudioEncoder&) = delete;

        using ReadableBuffer = std::span<const std::byte>;

        // The PCM layout the encoder has to be fed with, negotiated with the codec
        const PcmParameters& getInputParameters() const { return _inputParameters; }

        // Provide one buffer per channel if planar, or a single buffer containing all channels interleaved
        void writeSamples(std::span<const ReadableBuffer> inputChannelBuffers, std::size_t sampleCountPerChannel);

        // No more samples will be written: drain the encoder and finalize the container
        void flush();

        // Returns the number of bytes written, 0 meaning nothing is available yet
        std::size_t readBytes(std::span<std::byte> buffer);

        // flush() has been called and everything has been read back
        bool finished() const;

    private:
        void createOutputContext(const EncodeParameters& parameters);
        void createEncoder(const EncodeParameters& parameters, const AVCodec& encoder);
        void writeHeader(const EncodeParameters& parameters);
        void allocateBuffers();

        void encodePendingFrames();
        void encodeFrame(int sampleCount);
        void sendFrame(const AVFrame* frame);

        PcmParameters _inputParameters{};
        int _frameSampleCount{};
        std::int64_t _nextPts{};
        bool _flushed{};

        // Declared first: the muxer writes into it through the IO context, so it must outlive it
        std::vector<std::byte> _outputBuffer;
        std::size_t _outputReadIndex{};

        AVIOContextPtr _ioContext;
        AVFormatContextOutputPtr _formatContext;
        AVCodecContextPtr _encoderContext;
        AVStream* _stream{};
        AVFramePtr _frame;
        AVPacketPtr _packet;
        AVAudioFifoPtr _fifo;
    };
} // namespace lms::audio::ffmpeg
