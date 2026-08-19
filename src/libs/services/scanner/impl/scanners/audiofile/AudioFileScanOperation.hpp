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

#include <vector>

#include "core/media/ImageFormat.hpp"
#include "core/media/ImageType.hpp"

#include "audio/AudioProperties.hpp"
#include "image/Types.hpp"

#include "scanners/FileScanOperationBase.hpp"
#include "scanners/FileToScan.hpp"
#include "scanners/IFileScanOperation.hpp"

#include "types/TrackMetadata.hpp"

namespace lms::db
{
    class IDb;
} // namespace lms::db

namespace lms::scanner
{
    struct AudioFileInfoParserSet;
    class TrackMetadataParser;

    struct ImageInfo
    {
        std::size_t index;
        core::media::ImageType type{ core::media::ImageType::Unknown };
        std::uint64_t hash{};
        std::size_t size{};
        image::ImageDimensions dimensions;
        core::media::ImageFormat format;
        std::string description;
    };

    class AudioFileScanOperation : public FileScanOperationBase
    {
    public:
        AudioFileScanOperation(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings, const AudioFileInfoParserSet& audioFileInfoParserSet, const TrackMetadataParser& metadataParser);
        ~AudioFileScanOperation() override;
        AudioFileScanOperation(const AudioFileScanOperation&) = delete;
        AudioFileScanOperation& operator=(const AudioFileScanOperation&) = delete;

    private:
        core::LiteralString getName() const override { return "ScanAudioFile"; }
        void scan() override;
        OperationResult processResult() override;

        const AudioFileInfoParserSet& _audioFileInfoParserSet;
        const TrackMetadataParser& _metadataParser;

        struct AudioFileInfo
        {
            audio::AudioProperties audioProperties;
            Track track;
            std::vector<ImageInfo> images;
        };
        std::optional<AudioFileInfo> _file;
    };
} // namespace lms::scanner