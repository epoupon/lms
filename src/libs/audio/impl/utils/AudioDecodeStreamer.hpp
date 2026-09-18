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
#include <span>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>

#include "audio/utils/IAudioDecodeStreamer.hpp"

namespace lms::audio
{
    class IAudioOutputStream;
    class IAudioDecoder;
} // namespace lms::audio

namespace lms::audio::utils
{
    class AudioDecodeStreamer : public IAudioDecodeStreamer
    {
    public:
        AudioDecodeStreamer(boost::asio::io_context& ioContext, const AudioDecodeStreamerParameters& parameters);
        ~AudioDecodeStreamer() override;

        AudioDecodeStreamer(const AudioDecodeStreamer&) = delete;
        AudioDecodeStreamer& operator=(const AudioDecodeStreamer&) = delete;

    private:
        void start(const std::filesystem::path& path, std::chrono::microseconds offset, DecodeCompleteCallback cb) override;
        void abort() override;
        bool isComplete() const override;

        const audio::PcmParameters& getPcmParameters() const;

        void prepareBuffers(std::size_t bufferCount, std::chrono::microseconds bufferDuration);
        bool isWritePending() const;
        void decodeSome();
        std::size_t readSamples(std::span<std::byte> buffer);
        void onBufferWriteComplete(std::size_t bufferIndex);
        void notifyDecodeComplete();
        std::size_t sampleCountToByteCount(std::size_t sampleCount) const;

        struct BufferDesc
        {
            using Buffer = std::vector<std::byte>;
            Buffer buffer;
            bool isWritePending{};
        };

        boost::asio::io_context& _ioContext;
        boost::asio::io_context::strand _strand;
        audio::IAudioOutputStream& _outputStream;
        std::unique_ptr<audio::IAudioDecoder> _audioDecoder;

        std::vector<BufferDesc> _buffers;
        std::size_t _nextBufferIndex{};
        bool _eofReached{};
        bool _aborted{};

        DecodeCompleteCallback _decodeCompleteCallback;
    };
} // namespace lms::audio::utils