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

#include <optional>

#include "metadata/IAudioFileParser.hpp"

namespace lms::metadata
{
    class IImageReader;
    class ITagReader;

    class AudioFileParser : public IAudioFileParser
    {
    public:
        AudioFileParser(const AudioFileParserParameters& params = {});
        ~AudioFileParser() override = default;
        AudioFileParser(const AudioFileParser&) = delete;
        AudioFileParser& operator=(const AudioFileParser&) = delete;

    protected:
        std::unique_ptr<Track> parseMetaData(const std::filesystem::path& p) const override;
        std::unique_ptr<Track> parseMetaData(const ITagReader& reader) const;
        static void parseImages(const IImageReader& reader, ImageVisitor visitor);

    private:
        void parseImages(const std::filesystem::path& p, ImageVisitor visitor) const override;
        std::span<const std::filesystem::path> getSupportedExtensions() const override;

        void processTags(const ITagReader& reader, Track& track) const;

        std::optional<Medium> getMedium(const ITagReader& tagReader) const;
        std::optional<Release> getRelease(const ITagReader& tagReader) const;

        const AudioFileParserParameters _params;
    };
} // namespace lms::metadata
