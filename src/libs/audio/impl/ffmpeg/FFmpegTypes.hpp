
/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <memory>

extern "C"
{
    struct AVAudioFifo;
    struct AVChannelLayout;
    struct AVCodec;
    struct AVCodecContext;
    struct AVFormatContext;
    struct AVFrame;
    struct AVIOContext;
    struct AVOutputFormat;
    struct AVPacket;
    struct AVStream;

    struct SwrContext;
}

namespace lms::audio::ffmpeg
{
    struct AvCodecContextDeleter
    {
        void operator()(AVCodecContext* ctx) const noexcept;
    };
    using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AvCodecContextDeleter>;

    struct AvFormatContextDeleter
    {
        void operator()(AVFormatContext* ctx) const noexcept;
    };
    using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AvFormatContextDeleter>;

    // Output format contexts must not be closed using avformat_close_input
    struct AvFormatContextOutputDeleter
    {
        void operator()(AVFormatContext* ctx) const noexcept;
    };
    using AVFormatContextOutputPtr = std::unique_ptr<AVFormatContext, AvFormatContextOutputDeleter>;

    struct AvIOContextDeleter
    {
        void operator()(AVIOContext* ctx) const noexcept;
    };
    using AVIOContextPtr = std::unique_ptr<AVIOContext, AvIOContextDeleter>;

    struct AvAudioFifoDeleter
    {
        void operator()(AVAudioFifo* fifo) const noexcept;
    };
    using AVAudioFifoPtr = std::unique_ptr<AVAudioFifo, AvAudioFifoDeleter>;

    struct AvFrameDeleter
    {
        void operator()(AVFrame* frame) const noexcept;
    };
    using AVFramePtr = std::unique_ptr<AVFrame, AvFrameDeleter>;

    struct AVPacketDeleter
    {
        void operator()(AVPacket* packet) const noexcept;
    };
    using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

    struct SwrContextDeleter
    {
        void operator()(SwrContext* ctx) const noexcept;
    };
    using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
} // namespace lms::audio::ffmpeg