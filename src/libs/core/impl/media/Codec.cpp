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

#include "core/media/Codec.hpp"
#include "core/Exception.hpp"

#include <algorithm>
#include <array>

namespace lms::core::media
{
    namespace
    {
        constexpr std::array codecDescs{
            CodecDesc{ Codec::AAC, "AAC", "Advanced Audio Coding", false },
            CodecDesc{ Codec::AC3, "AC3", "Dolby AC-3", false },
            CodecDesc{ Codec::ALAC, "ALAC", "Apple Lossless Audio Codec", true },
            CodecDesc{ Codec::APE, "APE", "Monkey's Audio", true },
            CodecDesc{ Codec::DSD, "DSD", "Direct Stream Digital", true },
            CodecDesc{ Codec::EAC3, "E-AC3", "Dolby Digital Plus", false },
            CodecDesc{ Codec::FLAC, "FLAC", "Free Lossless Audio Codec", true },
            CodecDesc{ Codec::MP3, "MP3", "MPEG-1 Layer 3", false },
            CodecDesc{ Codec::MP4ALS, "MP4ALS", "MPEG-4 Audio Lossless Coding", true },
            CodecDesc{ Codec::MPC7, "MPC7", "Musepack7", false },
            CodecDesc{ Codec::MPC8, "MPC8", "Musepack8", false },
            CodecDesc{ Codec::Opus, "Opus", "Opus", false },
            CodecDesc{ Codec::PCM, "PCM", "Pulse-code modulation", true },
            CodecDesc{ Codec::Shorten, "Shorten", "Shorten", true },
            CodecDesc{ Codec::TrueAudio, "TTA", "The True Audio", true },
            CodecDesc{ Codec::Vorbis, "Vorbis", "Vorbis", false },
            CodecDesc{ Codec::WavPack, "WavPack", "WavPack", true },
            CodecDesc{ Codec::WMA1, "WMA1", "Windows Media Audio", false },
            CodecDesc{ Codec::WMA2, "WMA2", "Windows Media Audio 2", false },
            CodecDesc{ Codec::WMA9Pro, "WMA9Pro", "Windows Media Audio Professional", false },
            CodecDesc{ Codec::WMA9Lossless, "WMA9Lossless", "Windows Media Audio Lossless", true },
        };
    } // namespace

    void visitCodecs(const std::function<void(const CodecDesc&)>& visitor)
    {
        std::for_each(std::cbegin(codecDescs), std::cend(codecDescs), visitor);
    }

    const CodecDesc& getCodecDesc(Codec type)
    {
        const auto it{ std::find_if(codecDescs.begin(), codecDescs.end(), [type](const auto& desc) { return desc.type == type; }) };
        if (it == codecDescs.end()) [[unlikely]]
            throw LmsException{ "Unknown codec type" };

        return *it;
    }
} // namespace lms::core::media
