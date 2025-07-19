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

#include "FileScanOperationBase.hpp"
#include "ScannerSettings.hpp"
#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "metadata/Lyrics.hpp"
#include "services/scanner/ScanErrors.hpp"

#include "Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        class LyricsFileScanOperation : public FileScanOperationBase
        {
        public:
            using FileScanOperationBase::FileScanOperationBase;

        private:
            core::LiteralString getName() const override { return "ScanLyricsFile"; }
            void scan() override;
            OperationResult processResult() override;

            std::optional<metadata::Lyrics> _parsedLyrics;
        };

        void LyricsFileScanOperation::scan()
        {
            try
            {
                std::ifstream ifs{ getFilePath() };
                if (!ifs)
                {
                    const std::error_code ec{ errno, std::generic_category() };

                    addError<IOScanError>(getFilePath(), ec);
                    return;
                }

                _parsedLyrics = metadata::parseLyrics(ifs);
            }
            catch (const metadata::Exception& e)
            {
                addError<LyricsFileScanError>(getFilePath());
            }
        }

        LyricsFileScanOperation::OperationResult LyricsFileScanOperation::processResult()
        {
            db::Session& dbSession{ getDb().getTLSSession() };
            db::TrackLyrics::pointer trackLyrics{ db::TrackLyrics::find(dbSession, getFilePath()) };

            if (!_parsedLyrics)
            {
                if (trackLyrics)
                {
                    trackLyrics.remove();

                    LMS_LOG(DBUPDATER, DEBUG, "Removed lyrics file " << getFilePath());
                    return OperationResult::Removed;
                }

                return OperationResult::Skipped;
            }

            const bool added{ !trackLyrics };
            if (!trackLyrics)
            {
                trackLyrics = dbSession.create<db::TrackLyrics>();
                trackLyrics.modify()->setAbsoluteFilePath(getFilePath());
            }

            trackLyrics.modify()->setLastWriteTime(getLastWriteTime());
            trackLyrics.modify()->setFileSize(getFileSize());
            trackLyrics.modify()->setLanguage(!_parsedLyrics->language.empty() ? _parsedLyrics->language : "xxx");
            trackLyrics.modify()->setOffset(_parsedLyrics->offset);
            trackLyrics.modify()->setDisplayTitle(_parsedLyrics->displayTitle);
            trackLyrics.modify()->setDisplayArtist(_parsedLyrics->displayArtist);
            if (!_parsedLyrics->synchronizedLines.empty())
                trackLyrics.modify()->setSynchronizedLines(_parsedLyrics->synchronizedLines);
            else
                trackLyrics.modify()->setUnsynchronizedLines(_parsedLyrics->unsynchronizedLines);

            db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, getMediaLibrary().id) }; // may be null if settings are updated in // => next scan will correct this
            trackLyrics.modify()->setDirectory(utils::getOrCreateDirectory(dbSession, getFilePath().parent_path(), mediaLibrary));

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added external lyrics " << getFilePath());
                return OperationResult::Added;
            }

            LMS_LOG(DBUPDATER, DEBUG, "Updated external lyrics " << getFilePath());
            return OperationResult::Updated;
        }
    } // namespace

    LyricsFileScanner::LyricsFileScanner(db::IDb& db, ScannerSettings& _settings)
        : _db{ db }
        , _settings{ _settings }
    {
    }

    core::LiteralString LyricsFileScanner::getName() const
    {
        return "Lyrics scanner";
    }

    std::span<const std::filesystem::path> LyricsFileScanner::getSupportedFiles() const
    {
        return {};
    }

    std::span<const std::filesystem::path> LyricsFileScanner::getSupportedExtensions() const
    {
        return metadata::getSupportedLyricsFileExtensions();
    }

    bool LyricsFileScanner::needsScan(const FileToScan& file) const
    {
        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ _db.getTLSSession().createReadTransaction() };

        const db::TrackLyrics::pointer lyrics{ db::TrackLyrics::find(dbSession, file.filePath) };
        return !lyrics || lyrics->getLastWriteTime() != file.lastWriteTime;
    }

    std::unique_ptr<IFileScanOperation> LyricsFileScanner::createScanOperation(FileToScan&& fileToScan) const
    {
        return std::make_unique<LyricsFileScanOperation>(std::move(fileToScan), _db, _settings);
    }
} // namespace lms::scanner
