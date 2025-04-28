/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "IFileScanOperation.hpp"

#include <memory>
#include <vector>

#include "image/Types.hpp"
#include "metadata/IAudioFileParser.hpp"

#include "FileToScan.hpp"
#include "IFileScanner.hpp"

namespace lms::db
{
    class Db;
} // namespace lms::db

namespace lms::scanner
{
    struct ImageInfo
    {
        std::size_t index;
        metadata::Image::Type type{ metadata::Image::Type::Unknown };
        std::uint64_t hash{};
        std::size_t size{};
        image::ImageProperties properties;
        std::string mimeType;
        std::string description;
    };

    class AudioFileScanOperation : public IFileScanOperation
    {
    public:
        AudioFileScanOperation(const FileToScan& fileToScan, db::Db& db, metadata::IAudioFileParser& parser, const ScannerSettings& settings)
            : _file{ fileToScan.file }
            , _mediaLibrary{ fileToScan.mediaLibrary }
            , _db{ db }
            , _parser{ parser }
            , _settings{ settings }
        {
        }
        ~AudioFileScanOperation() override = default;
        AudioFileScanOperation(const AudioFileScanOperation&) = delete;
        AudioFileScanOperation& operator=(const AudioFileScanOperation&) = delete;

    private:
        const std::filesystem::path& getFile() const override { return _file; };
        core::LiteralString getName() const override { return "ScanAudioFile"; }
        void scan() override;
        void processResult(ScanContext& context) override;

        const std::filesystem::path _file;
        const MediaLibraryInfo _mediaLibrary;
        db::Db& _db;
        metadata::IAudioFileParser& _parser;
        const ScannerSettings& _settings;
        std::unique_ptr<metadata::Track> _parsedTrack;
        std::vector<ImageInfo> _parsedImages;
    };

} // namespace lms::scanner