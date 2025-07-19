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

#include "AudioFileScanner.hpp"

#include "core/IConfig.hpp"
#include "core/Service.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Track.hpp"
#include "metadata/IAudioFileParser.hpp"

#include "AudioFileScanOperation.hpp"
#include "ScannerSettings.hpp"
#include "Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        metadata::ParserReadStyle getParserReadStyle()
        {
            std::string_view readStyle{ core::Service<core::IConfig>::get()->getString("scanner-parser-read-style", "average") };

            if (readStyle == "fast")
                return metadata::ParserReadStyle::Fast;
            if (readStyle == "average")
                return metadata::ParserReadStyle::Average;
            if (readStyle == "accurate")
                return metadata::ParserReadStyle::Accurate;

            throw core::LmsException{ "Invalid value for 'scanner-parser-read-style'" };
        }

        metadata::AudioFileParserParameters createAudioFileParserParameters(const ScannerSettings& settings)
        {
            metadata::AudioFileParserParameters params;
            params.userExtraTags = settings.extraTags;
            params.artistTagDelimiters = settings.artistTagDelimiters;
            params.defaultTagDelimiters = settings.defaultTagDelimiters;
            params.artistsToNotSplit.insert(settings.artistsToNotSplit.cbegin(), settings.artistsToNotSplit.end());
            params.backend = metadata::ParserBackend::TagLib;
            params.readStyle = getParserReadStyle();

            return params;
        }

    } // namespace

    AudioFileScanner::AudioFileScanner(db::IDb& db, const ScannerSettings& settings)
        : _db{ db }
        , _settings{ settings }
        , _metadataParser{ metadata::createAudioFileParser(createAudioFileParserParameters(settings)) } // For now, always use TagLib
    {
    }

    AudioFileScanner::~AudioFileScanner() = default;

    core::LiteralString AudioFileScanner::getName() const
    {
        return "Audio scanner";
    }

    std::span<const std::filesystem::path> AudioFileScanner::getSupportedFiles() const
    {
        return {};
    }

    std::span<const std::filesystem::path> AudioFileScanner::getSupportedExtensions() const
    {
        return _metadataParser->getSupportedExtensions();
    }

    bool AudioFileScanner::needsScan(const FileToScan& file) const
    {
        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        std::optional<db::FileInfo> fileInfo{ db::Track::findFileInfo(dbSession, file.filePath) };
        return !fileInfo
            || fileInfo->lastWrittenTime != file.lastWriteTime
            || fileInfo->scanVersion != _settings.audioScanVersion;
    }

    std::unique_ptr<IFileScanOperation> AudioFileScanner::createScanOperation(FileToScan&& fileToScan) const
    {
        return std::make_unique<AudioFileScanOperation>(std::move(fileToScan), _db, _settings, *_metadataParser);
    }
} // namespace lms::scanner
