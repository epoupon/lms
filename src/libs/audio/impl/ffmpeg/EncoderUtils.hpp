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

extern "C"
{
#include <libavutil/samplefmt.h>
}

#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

#include "audio/PcmTypes.hpp"

#include "FFmpegTypes.hpp"

namespace lms::audio::ffmpeg::utils
{
    // Throw if the container cannot be muxed
    const char* getMuxerName(core::media::Container container);

    // Throw if no encoder is available for this codec
    const AVCodec* findEncoder(core::media::Codec codec);

    // These pick the closest configuration the encoder actually supports
    ::AVSampleFormat pickSampleFormat(const AVCodec& encoder, PcmSampleType desiredSampleType);
    int pickSampleRate(const AVCodec& encoder, unsigned desiredSampleRate);
    void pickChannelLayout(const AVCodec& encoder, unsigned desiredChannelCount, AVChannelLayout& layout);
} // namespace lms::audio::ffmpeg::utils
