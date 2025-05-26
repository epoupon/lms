/*
 * Copyright (C) 2018 Emeric Poupon
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
#include <functional>
#include <memory>
#include <span>

#include "metadata/Exception.hpp"
#include "metadata/Types.hpp"

namespace lms::metadata
{
    class IAudioFileParser
    {
    public:
        virtual ~IAudioFileParser() = default;

        virtual std::unique_ptr<Track> parseMetaData(const std::filesystem::path& p) const = 0;

        using ImageVisitor = std::function<void(const Image&)>;
        virtual void parseImages(const std::filesystem::path& p, ImageVisitor visitor) const = 0;

        virtual std::span<const std::filesystem::path> getSupportedExtensions() const = 0;
    };

    std::unique_ptr<IAudioFileParser> createAudioFileParser(const AudioFileParserParameters& params);
} // namespace lms::metadata
