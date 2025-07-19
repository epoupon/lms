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
#include "metadata/Types.hpp"

#include "FileScanOperationBase.hpp"
#include "FileToScan.hpp"

namespace lms::db
{
    class IDb;
} // namespace lms::db

namespace lms::metadata
{
    class IAudioFileParser;
} // namespace lms::metadata

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

    class AudioFileScanOperation : public FileScanOperationBase
    {
    public:
        AudioFileScanOperation(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings, metadata::IAudioFileParser& parser);
        ~AudioFileScanOperation() override;
        AudioFileScanOperation(const AudioFileScanOperation&) = delete;
        AudioFileScanOperation& operator=(const AudioFileScanOperation&) = delete;

    private:
        core::LiteralString getName() const override { return "ScanAudioFile"; }
        void scan() override;
        OperationResult processResult() override;

        metadata::IAudioFileParser& _parser;
        std::unique_ptr<metadata::Track> _parsedTrack;
        std::vector<ImageInfo> _parsedImages;
    };

} // namespace lms::scanner