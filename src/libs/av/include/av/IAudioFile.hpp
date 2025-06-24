/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lms::av
{
    // List should be sync with the codecs shipped in the lms's docker version
    enum class DecodingCodec
    {
        UNKNOWN,
        MP3,
        AAC,
        AC3,
        VORBIS,
        WMAV1,
        WMAV2,
        FLAC,      // Flac
        ALAC,      // Apple Lossless Audio Codec (ALAC)
        WAVPACK,   // WavPack
        MUSEPACK7, // Musepack
        MUSEPACK8,
        APE,             // Monkey's Audio
        EAC3,            // Enhanced AC-3
        MP4ALS,          // MPEG-4 Audio Lossless Coding
        OPUS,            // Opus
        SHORTEN,         // Shorten (shn)
        DSD_LSBF,        // DSD (Direct Stream Digital), least significant bit first
        DSD_LSBF_PLANAR, // DSD (Direct Stream Digital), least significant bit first, planar
        DSD_MSBF,        // DSD (Direct Stream Digital), most significant bit first
        DSD_MSBF_PLANAR, // DSD (Direct Stream Digital), most significant bit first, planar
        // TODO add PCM codecs
    };

    struct Picture
    {
        std::string mimeType;
        std::span<const std::byte> data; // valid as long as IAudioFile exists
    };

    struct ContainerInfo
    {
        std::size_t bitrate{};
        std::string name;
        std::chrono::milliseconds duration{};
    };

    struct StreamInfo
    {
        size_t index{};
        std::size_t bitrate{};
        std::size_t bitsPerSample{};
        std::size_t channelCount{};
        std::size_t sampleRate{};
        DecodingCodec codec;
        std::string codecName;
    };

    class IAudioFile
    {
    public:
        virtual ~IAudioFile() = default;

        // Keys are forced to be in upper case
        using MetadataMap = std::unordered_map<std::string, std::string>;

        virtual const std::filesystem::path& getPath() const = 0;
        virtual ContainerInfo getContainerInfo() const = 0;
        virtual MetadataMap getMetaData() const = 0;
        virtual std::vector<StreamInfo> getStreamInfo() const = 0;
        virtual std::optional<StreamInfo> getBestStreamInfo() const = 0;   // none if failure/unknown
        virtual std::optional<std::size_t> getBestStreamIndex() const = 0; // none if failure/unknown
        virtual bool hasAttachedPictures() const = 0;
        virtual void visitAttachedPictures(std::function<void(const Picture&, const MetadataMap& metadata)> func) const = 0;
    };

    std::unique_ptr<IAudioFile> parseAudioFile(const std::filesystem::path& p);
} // namespace lms::av
