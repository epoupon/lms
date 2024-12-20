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

#include "LyricsFileScanner.hpp"

#include <fstream>
#include <optional>

#include "core/ILogger.hpp"
#include "database/Db.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Session.hpp"
#include "database/TrackLyrics.hpp"
#include "metadata/Lyrics.hpp"

#include "IFileScanOperation.hpp"
#include "ScanContext.hpp"
#include "Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        class LyricsFileScanOperation : public IFileScanOperation
        {
        public:
            LyricsFileScanOperation(const FileToScan& file, db::Db& db)
                : _file{ file.file }
                , _mediaLibrary{ file.mediaLibrary }
                , _db{ db } {}

        private:
            const std::filesystem::path& getFile() const override { return _file; };
            core::LiteralString getName() const override { return "ScanLyricsFile"; }
            void scan() override;
            void processResult(ScanContext& context) override;

            const std::filesystem::path _file;
            const MediaLibraryInfo _mediaLibrary;
            db::Db& _db;

            std::optional<metadata::Lyrics> _parsedLyrics;
        };

        void LyricsFileScanOperation::scan()
        {
            try
            {
                std::ifstream ifs{ _file.string() };
                if (!ifs)
                    LMS_LOG(DBUPDATER, ERROR, "Cannot open file '" << _file.string() << "'");
                else
                    _parsedLyrics = metadata::parseLyrics(ifs);
            }
            catch (const metadata::Exception& e)
            {
                LMS_LOG(DBUPDATER, ERROR, "Cannot read lyrics in file '" << _file.string() << "': " << e.what());
            }
        }

        void LyricsFileScanOperation::processResult(ScanContext& context)
        {
            ScanStats& stats{ context.stats };

            const std::optional<FileInfo> fileInfo{ utils::retrieveFileInfo(_file, _mediaLibrary.rootDirectory) };
            if (!fileInfo)
            {
                stats.skips++;
                return;
            }

            db::Session& dbSession{ _db.getTLSSession() };
            db::TrackLyrics::pointer trackLyrics{ db::TrackLyrics::find(dbSession, _file) };

            if (!_parsedLyrics)
            {
                if (trackLyrics)
                {
                    trackLyrics.remove();
                    stats.deletions++;
                }
                context.stats.errors.emplace_back(_file, ScanErrorType::CannotReadLyricsFile);
                return;
            }

            const bool added{ !trackLyrics };
            if (!trackLyrics)
            {
                trackLyrics = dbSession.create<db::TrackLyrics>();
                trackLyrics.modify()->setAbsoluteFilePath(_file);
            }

            trackLyrics.modify()->setLastWriteTime(fileInfo->lastWriteTime);
            trackLyrics.modify()->setFileSize(fileInfo->fileSize);
            trackLyrics.modify()->setLanguage(!_parsedLyrics->language.empty() ? _parsedLyrics->language : "xxx");
            trackLyrics.modify()->setOffset(_parsedLyrics->offset);
            trackLyrics.modify()->setDisplayTitle(_parsedLyrics->displayTitle);
            trackLyrics.modify()->setDisplayArtist(_parsedLyrics->displayArtist);
            if (!_parsedLyrics->synchronizedLines.empty())
                trackLyrics.modify()->setSynchronizedLines(_parsedLyrics->synchronizedLines);
            else
                trackLyrics.modify()->setUnsynchronizedLines(_parsedLyrics->unsynchronizedLines);

            db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, _mediaLibrary.id) }; // may be null if settings are updated in // => next scan will correct this
            trackLyrics.modify()->setDirectory(utils::getOrCreateDirectory(dbSession, _file.parent_path(), mediaLibrary));

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added external lyrics '" << _file.string() << "'");
                stats.additions++;
            }
            else
            {
                LMS_LOG(DBUPDATER, DEBUG, "Updated external lyrics '" << _file.string() << "'");
                stats.updates++;
            }
        }
    } // namespace

    LyricsFileScanner::LyricsFileScanner(db::Db& db)
        : _db{ db }
    {
    }

    core::LiteralString LyricsFileScanner::getName() const
    {
        return "Lyrics scanner";
    }

    std::span<const std::filesystem::path> LyricsFileScanner::getSupportedExtensions() const
    {
        return metadata::getSupportedLyricsFileExtensions();
    }

    bool LyricsFileScanner::needsScan(ScanContext& context, const FileToScan& file) const
    {
        ScanStats& stats{ context.stats };

        const Wt::WDateTime lastWriteTime{ utils::retrieveFileGetLastWrite(file.file) };
        // Should rarely fail as we are currently iterating it
        if (!lastWriteTime.isValid())
        {
            stats.skips++;
            return false;
        }

        if (!context.scanOptions.fullScan)
        {
            db::Session& dbSession{ _db.getTLSSession() };
            auto transaction{ _db.getTLSSession().createReadTransaction() };

            const db::TrackLyrics::pointer lyrics{ db::TrackLyrics::find(dbSession, file.file) };
            if (lyrics && lyrics->getLastWriteTime() == lastWriteTime)
            {
                stats.skips++;
                return false;
            }
        }

        return true; // need to scan
    }

    std::unique_ptr<IFileScanOperation> LyricsFileScanner::createScanOperation(const FileToScan& fileToScan) const
    {
        return std::make_unique<LyricsFileScanOperation>(fileToScan, _db);
    }
} // namespace lms::scanner
