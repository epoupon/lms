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
#include "database/Db.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "AudioFileScanOperation.hpp"
#include "ScanContext.hpp"
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
            params.backend = metadata::ParserBackend::TagLib;
            params.readStyle = getParserReadStyle();

            return params;
        }

    } // namespace
    AudioFileScanner::AudioFileScanner(db::Db& db, const ScannerSettings& settings)
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

    std::span<const std::filesystem::path> AudioFileScanner::getSupportedExtensions() const
    {
        return _metadataParser->getSupportedExtensions();
    }

    bool AudioFileScanner::needsScan(ScanContext& context, const FileToScan& file) const
    {
        ScanStats& stats{ context.stats };

        const Wt::WDateTime lastWriteTime{ utils::retrieveFileGetLastWrite(file.file) };
        // Should rarely fail as we are currently iterating it
        if (!lastWriteTime.isValid())
        {
            stats.skips++;
            return false;
        }

        if (context.scanOptions.fullScan)
            return true;

        bool needUpdateLibrary{};
        db::Session& dbSession{ _db.getTLSSession() };

        {
            auto transaction{ dbSession.createReadTransaction() };

            // Skip file if last write is the same
            const db::Track::pointer track{ db::Track::findByPath(dbSession, file.file) };
            if (track
                && track->getLastWriteTime() == lastWriteTime
                && track->getScanVersion() == _settings.audioScanVersion)
            {
                // this file may have been moved from one library to another, then we just need to update the media library id instead of a full rescan
                const auto trackMediaLibrary{ track->getMediaLibrary() };
                if (trackMediaLibrary && trackMediaLibrary->getId() == file.mediaLibrary.id)
                {
                    stats.skips++;
                    return false;
                }

                needUpdateLibrary = true;
            }
        }

        if (needUpdateLibrary)
        {
            auto transaction{ dbSession.createWriteTransaction() };

            db::Track::pointer track{ db::Track::findByPath(dbSession, file.file) };
            assert(track);
            track.modify()->setMediaLibrary(db::MediaLibrary::find(dbSession, file.mediaLibrary.id)); // may be null, will be handled in the next scan anyway
            stats.updates++;
            return false;
        }

        return true; // need to scan
    }

    std::unique_ptr<IFileScanOperation> AudioFileScanner::createScanOperation(const FileToScan& fileToScan) const
    {
        return std::make_unique<AudioFileScanOperation>(fileToScan, _db, *_metadataParser, _settings);
    }
} // namespace lms::scanner
