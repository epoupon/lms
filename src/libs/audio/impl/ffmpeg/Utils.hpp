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

    bool isDemuxingSupported(core::media::Container container);
    bool isDecodingSupported(core::media::Codec codec);
    bool isMuxingSupported(core::media::Container container);
    bool isEncodingSupported(core::media::Codec codec);

    bool isCodecMuxingSupported(core::media::Container container, core::media::Codec codec);

    // nullptr if not supported by this build (see isEncodingSupported/isMuxingSupported)
    const AVCodec* getEncoderForCodec(core::media::Codec codec);
    const char* getMuxerNameForContainer(core::media::Container container);

    std::optional<core::media::Container> containerFromFormatName(std::string_view name);
    std::optional<core::media::Codec> codecFromAVCodecId(AVCodecID codec);

    PcmSampleType toPcmSampleType(::AVSampleFormat format);
    ::AVSampleFormat toAvSampleFormat(PcmSampleType type, bool planar);

    void init();
    bool isInit();
} // namespace lms::audio::ffmpeg::utils
