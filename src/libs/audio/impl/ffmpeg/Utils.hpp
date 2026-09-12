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

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

extern "C"
{
#include <libavcodec/codec_id.h>
#include <libavutil/samplefmt.h>
}

#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

#include "audio/PcmTypes.hpp"

#include "FFmpegTypes.hpp"

namespace lms::audio::ffmpeg::utils
{
    std::string averrorToString(int error);

    std::span<const std::filesystem::path> getSupportedDemuxerExtensions();

    bool isDecodingSupported(core::media::Container container, core::media::Codec codec);
    bool isCodecMuxingSupported(core::media::Container container, core::media::Codec codec);

    // Throw if no encoder is available for this codec
    const AVCodec* findEncoder(core::media::Codec codec);

    // Throw if this specific container/codec pairing cannot be muxed
    const AVOutputFormat* findMuxer(core::media::Container container, core::media::Codec codec);

    // Empty means the encoder does not restrict this (or there is no encoder for this codec)
    std::span<const ::AVSampleFormat> getSupportedSampleFormats(core::media::Codec codec);
    std::span<const int> getSupportedSampleRates(core::media::Codec codec);
    std::span<const AVChannelLayout* const> getSupportedChannelLayouts(core::media::Codec codec);

    std::optional<core::media::Container> containerFromDemuxerName(const char* name);
    std::optional<core::media::Codec> codecFromAVCodecId(AVCodecID codec);

    PcmSampleType toPcmSampleType(::AVSampleFormat format);
    ::AVSampleFormat toAvSampleFormat(PcmSampleType type, bool planar);

    void init();
    bool isInit();
} // namespace lms::audio::ffmpeg::utils
