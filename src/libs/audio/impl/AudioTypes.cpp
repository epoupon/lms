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

#include "audio/AudioTypes.hpp"

namespace lms::audio
{
    core::LiteralString containerTypeToString(ContainerType type)
    {
        switch (type)
        {
        case ContainerType::AIFF:
            return "AIFF";
        case ContainerType::APE:
            return "APE";
        case ContainerType::ASF:
            return "ASF";
        case ContainerType::DSF:
            return "DSF";
        case ContainerType::FLAC:
            return "FLAC";
        case ContainerType::MP4:
            return "MP4";
        case ContainerType::MPC:
            return "MPC";
        case ContainerType::MPEG:
            return "MPEG";
        case ContainerType::Ogg:
            return "Ogg";
        case ContainerType::Shorten:
            return "Shorten";
        case ContainerType::TrueAudio:
            return "TrueAudio";
        case ContainerType::WAV:
            return "WAV";
        case ContainerType::WavPack:
            return "WavPack";
        }

        return "";
    }

    core::LiteralString codecTypeToString(CodecType type)
    {
        switch (type)
        {
        case CodecType::AAC:
            return "AAC";
        case CodecType::ALAC:
            return "ALAC";
        case CodecType::APE:
            return "APE";
        case CodecType::DSD:
            return "DSD";
        case CodecType::FLAC:
            return "FLAC";
        case CodecType::MP3:
            return "MP3";
        case CodecType::MP4ALS:
            return "MP4ALS";
        case CodecType::MPC7:
            return "MPC7";
        case CodecType::MPC8:
            return "MPC8";
        case CodecType::Opus:
            return "Opus";
        case CodecType::PCM:
            return "PCM";
        case CodecType::Shorten:
            return "Shorten";
        case CodecType::TrueAudio:
            return "TrueAudio";
        case CodecType::Vorbis:
            return "Vorbis";
        case CodecType::WavPack:
            return "WavPack";
        case CodecType::WMA1:
            return "WMA1";
        case CodecType::WMA2:
            return "WMA2";
        case CodecType::WMA9Pro:
            return "WMA9Pro";
        case CodecType::WMA9Lossless:
            return "WMA9Lossless";
        }

        return "";
    }

    bool isCodecLossless(CodecType type)
    {
        switch (type)
        {
        case CodecType::ALAC:
        case CodecType::APE:
        case CodecType::DSD:
        case CodecType::FLAC:
        case CodecType::MP4ALS:
        case CodecType::PCM:
        case CodecType::Shorten:
        case CodecType::TrueAudio:
        case CodecType::WavPack:
        case CodecType::WMA9Lossless:
            return true;

        case CodecType::AAC:
        case CodecType::MP3:
        case CodecType::MPC7:
        case CodecType::MPC8:
        case CodecType::Opus:
        case CodecType::Vorbis:
        case CodecType::WMA1:
        case CodecType::WMA2:
        case CodecType::WMA9Pro:
            return false;
        }

        return false;
    }
} // namespace lms::audio