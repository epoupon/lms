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

#include <functional>
#include <optional>
#include <string_view>

#include "core/LiteralString.hpp"

namespace lms::core::media
{
    enum class Codec
    {
        AAC,
        AC3,
        ALAC, // Apple Lossless Audio Codec (ALAC)
        APE,  // Monkey's Audio
        DSD,  // DSD
        EAC3,
        FLAC, // Flac
        MP3,
        MP4ALS, // MPEG-4 Audio Lossless Coding
        MPC7,   // Musepack
        MPC8,   // Musepack
        Opus,   // Opus
        PCM,
        Shorten, // Shorten (shn)
        TrueAudio,
        Vorbis,
        WavPack, // WavPack
        WMA1,
        WMA2,
        WMA9Pro,
        WMA9Lossless,
    };

    struct CodecDesc
    {
        Codec type;
        core::LiteralString name;
        core::LiteralString longName;
        bool isLossless;
    };
    void visitCodecs(const std::function<void(const CodecDesc&)>& visitor);
    const CodecDesc& getCodecDesc(Codec type);
    std::optional<Codec> findCodec(std::string_view name); // name as in CodecDesc::name
} // namespace lms::core::media
