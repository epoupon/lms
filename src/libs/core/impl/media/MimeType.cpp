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

#include "core/media/MimeType.hpp"

namespace lms::core::media
{
    core::LiteralString getMimeType(Container container, Codec codec)
    {
        switch (container)
        {
        case Container::AIFF:
            return "audio/x-aiff";
        case Container::APE:
            return "audio/x-monkeys-audio";
        case Container::ASF:
            return "audio/x-ms-wma";
        case Container::DSF:
            return "audio/x-dsd-dsf";
        case Container::FLAC:
            return "audio/flac";
        case Container::MP4:
            switch (codec)
            {
            case Codec::AAC:
                return "audio/mp4; codecs=\"mp4a.40.2\"";
            case Codec::ALAC:
                return "audio/mp4; codecs=\"alac\"";
            case Codec::MP4ALS:
                return "audio/mp4; codecs=\"mp4als\"";
            default:
                return "audio/mp4";
            }

        case Container::MPC:
            return "audio/x-musepack";
        case Container::MPEG:
            return "audio/mpeg";
        case Container::Ogg:
            switch (codec)
            {
            case Codec::Opus:
                return "audio/ogg; codecs=\"opus\"";
            case Codec::Vorbis:
                return "audio/ogg; codecs=\"vorbis\"";
            case Codec::FLAC:
                return "audio/ogg; codecs=\"flac\"";
            default:
                return "audio/ogg";
            }

        case Container::Shorten:
            return "audio/x-shn";
        case Container::TrueAudio:
            return "audio/x-tta";
        case Container::WAV:
            return "audio/wav";
        case Container::WavPack:
            return "audio/x-wavpack";
        }

        return "application/octet-stream";
    }

    core::LiteralString getMimeType(ImageFormat format)
    {
        switch (format)
        {
        case ImageFormat::BMP:
            return "image/bmp";
        case ImageFormat::GIF:
            return "image/gif";
        case ImageFormat::JPEG:
            return "image/jpeg";
        case ImageFormat::PNG:
            return "image/png";
        case ImageFormat::SVG:
            return "image/svg+xml";
        case ImageFormat::WebP:
            return "image/webp";
        }

        return "application/octet-stream";
    }
} // namespace lms::core::media