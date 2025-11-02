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

#include <filesystem>
#include <span>

#include "audio/Exception.hpp"

namespace lms::audio
{
    class AudioProperties;
    class IImageReader;
    class ITagReader;

    class IAudioFileInfo
    {
    public:
        virtual ~IAudioFileInfo() = default;

        virtual const AudioProperties& getAudioProperties() const = 0;
        virtual const IImageReader& getImageReader() const = 0;
        virtual const ITagReader& getTagReader() const = 0;
    };

    class AudioFileParsingException : public Exception
    {
    public:
        AudioFileParsingException(const std::filesystem::path& path, std::string_view error = "")
            : Exception{ error }, _path{ path } {}

        const std::filesystem::path& getPath() const { return _path; }

    private:
        std::filesystem::path _path;
    };

    class AudioFileNoAudioPropertiesException : public AudioFileParsingException
    {
    public:
        using AudioFileParsingException::AudioFileParsingException;
    };

    class IOException : public Exception
    {
    public:
        IOException(std::string_view message, std::error_code err)
            : Exception{ std::string{ message } + ": " + err.message() }
            , _err{ err }
        {
        }

        std::error_code getErrorCode() const { return _err; }

    private:
        std::error_code _err;
    };

    struct ParserOptions
    {
        enum class Parser
        {
            TagLib,
            FFmpeg,
        };

        enum class AudioPropertiesReadStyle
        {
            Fast,
            Average,
            Accurate,
        };

        Parser parser{ Parser::TagLib };
        AudioPropertiesReadStyle readStyle{ AudioPropertiesReadStyle::Average };
        bool enableExtraDebugLogs{};
    };
    std::unique_ptr<IAudioFileInfo> parseAudioFile(const std::filesystem::path& p, const ParserOptions& parserOptions = ParserOptions{});

    std::span<const std::filesystem::path> getSupportedExtensions(ParserOptions::ParserOptions::Parser parser);
} // namespace lms::audio
