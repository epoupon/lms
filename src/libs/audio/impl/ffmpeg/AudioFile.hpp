/*
 * Copyright (C) 2020 Emeric Poupon
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
#include <vector>

#include "audio/AudioTypes.hpp"

extern "C"
{
    struct AVFormatContext;
}

namespace lms::audio::ffmpeg
{
    struct Picture
    {
        std::string mimeType;
        std::span<const std::byte> data; // valid as long as IAudioFile exists
    };

    struct ContainerInfo
    {
        std::optional<ContainerType> container;
        std::string containerName;

        std::size_t bitrate{};
        std::chrono::milliseconds duration{};
    };

    struct StreamInfo
    {
        size_t index{};
        std::optional<CodecType> codec;
        std::string codecName;

        std::optional<size_t> bitrate;
        std::optional<std::size_t> bitsPerSample;
        std::optional<std::size_t> channelCount;
        std::optional<std::size_t> sampleRate;
    };

    class AudioFile
    {
    public:
        AudioFile(const std::filesystem::path& p);
        ~AudioFile();
        AudioFile(const AudioFile&) = delete;
        AudioFile& operator=(const AudioFile&) = delete;

        using MetadataMap = std::unordered_map<std::string, std::string>;

        const std::filesystem::path& getPath() const;
        ContainerInfo getContainerInfo() const;
        MetadataMap getMetaData() const;
        std::vector<StreamInfo> getStreamInfo() const;
        std::optional<StreamInfo> getBestStreamInfo() const;
        std::optional<std::size_t> getBestStreamIndex() const;
        bool hasAttachedPictures() const;
        void visitAttachedPictures(std::function<void(const Picture&, const MetadataMap&)> func) const;

    private:
        std::optional<StreamInfo> getStreamInfo(std::size_t streamIndex) const;

        const std::filesystem::path _p;
        AVFormatContext* _context{};
    };
} // namespace lms::audio::ffmpeg