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

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace lms::api::subsonic
{
    struct DirectPlayProfile
    {
        std::vector<std::string> containers;         // The container format (e.g., mp3, flac).
        std::vector<std::string> audioCodecs;        // Comma-separated list of supported audio codecs.
        std::string protocol;                        // The streaming protocol. Can be http or hls.
        std::optional<std::size_t> maxAudioChannels; // The maximum number of audio channels supported.
    };

    struct TranscodingProfile
    {
        std::string container;                       // The container format (e.g., mp3, flac).
        std::string audioCodec;                      // The target audio codec for transcoding.
        std::string protocol;                        // The streaming protocol. Can be http or hls.
        std::optional<std::size_t> maxAudioChannels; // The maximum number of audio channels for the transcoded stream.
    };

    struct Limitation
    {
        enum class Type
        {
            AudioChannels,
            AudioBitrate,
            AudioProfile,
            AudioSamplerate,
            AudioBitdepth,
        };

        enum class ComparisonOperator : unsigned char
        {
            Equals,
            NotEquals,
            LessThanEqual,
            GreaterThanEqual,
            EqualsAny,
            NotEqualsAny,
        };

        Type name;                       // The name of the limitation. Can be audioChannels, audioBitrate, audioProfile, audioSamplerate, or audioBitdepth.
        ComparisonOperator comparison;   // The comparison operator. Can be Equals, NotEquals, LessThanEqual, GreaterThanEqual, EqualsAny, or NotEqualsAny.
        std::vector<std::string> values; // The value to compare against. For EqualsAny and NotEqualsAny, this should be a pipe-separated (|) list of values (e.g., 44100|48000).
        bool required;                   // Whether this limitation must be met.
    };

    struct CodecProfile
    {
        std::string type;                    // The type of codec profile. Currently only AudioCodec is supported
        std::string name;                    // The name of the codec (e.g., mp3, flac).
        std::vector<Limitation> limitations; // A list of limitations for this codec.
    };

    struct ClientInfo
    {
        std::string name;                                    // The name of the client device
        std::string platform;                                // The platform of the client (e.g., Android, iOS).
        std::optional<unsigned> maxAudioBitrate;             // The maximum audio bitrate the client can handle.
        std::optional<unsigned> maxTranscodingAudioBitrate;  // The maximum audio bitrate for transcoded content.
        std::vector<DirectPlayProfile> directPlayProfiles;   // A list of profiles for direct playback.
        std::vector<TranscodingProfile> transcodingProfiles; // A list of profiles for transcoding. The server should evaluate these in the order they are listed, as a priority list.
        std::vector<CodecProfile> codecProfiles;             // A list of codec-specific profiles.
    };

    [[nodiscard]] ClientInfo parseClientInfoFromJson(std::istream& is);
} // namespace lms::api::subsonic