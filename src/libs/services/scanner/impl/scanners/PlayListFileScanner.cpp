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

#include "PlayListFileScanner.hpp"

#include <fstream>
#include <optional>

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/PlayListFile.hpp"
#include "metadata/Exception.hpp"
#include "metadata/PlayList.hpp"

#include "FileScanOperationBase.hpp"
#include "ScanContext.hpp"
#include "Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        class PlayListFileScanOperation : public FileScanOperationBase
        {
        public:
            using FileScanOperationBase::FileScanOperationBase;
            ~PlayListFileScanOperation() override = default;
            PlayListFileScanOperation(const PlayListFileScanOperation&) = delete;
            PlayListFileScanOperation& operator=(const PlayListFileScanOperation&) = delete;

        private:
            core::LiteralString getName() const override { return "ScanPlayListFile"; }
            void scan() override;
            OperationResult processResult() override;

            std::optional<metadata::PlayList> _parsedPlayList;
        };

        void PlayListFileScanOperation::scan()
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

                _parsedPlayList = metadata::parsePlayList(ifs);
            }
            catch (const metadata::Exception& e)
            {
                addError<PlayListFileScanError>(getFilePath());
            }
        }

        PlayListFileScanOperation::OperationResult PlayListFileScanOperation::processResult()
        {
            db::Session& dbSession{ getDb().getTLSSession() };
            db::PlayListFile::pointer playList{ db::PlayListFile::find(dbSession, getFilePath()) };

            if (!_parsedPlayList)
            {
                if (playList)
                {
                    playList.remove();

                    LMS_LOG(DBUPDATER, DEBUG, "Removed playlist file " << getFilePath());
                    return OperationResult::Removed;
                }

                return OperationResult::Skipped;
            }

            const bool added{ !playList };
            if (!playList)
                playList = dbSession.create<db::PlayListFile>(getFilePath());

            playList.modify()->setLastWriteTime(getLastWriteTime());
            playList.modify()->setFileSize(getFileSize());
            if (!_parsedPlayList->name.empty())
                playList.modify()->setName(_parsedPlayList->name);
            else
                playList.modify()->setName(getFilePath().stem().string());
            playList.modify()->setFiles(_parsedPlayList->files);

            db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, getMediaLibrary().id) }; // may be null if settings are updated in // => next scan will correct this
            playList.modify()->setDirectory(utils::getOrCreateDirectory(dbSession, getFilePath().parent_path(), mediaLibrary));

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added playlist file " << getFilePath());
                return OperationResult::Added;
            }
            else
            {
                LMS_LOG(DBUPDATER, DEBUG, "Updated playlist file '" << getFilePath());
                return OperationResult::Updated;
            }
        }
    } // namespace

    PlayListFileScanner::PlayListFileScanner(db::IDb& db, ScannerSettings& settings)
        : _db{ db }
        , _settings{ settings }
    {
    }

    core::LiteralString PlayListFileScanner::getName() const
    {
        return "PlayList scanner";
    }

    std::span<const std::filesystem::path> PlayListFileScanner::getSupportedFiles() const
    {
        return {};
    }

    std::span<const std::filesystem::path> PlayListFileScanner::getSupportedExtensions() const
    {
        return metadata::getSupportedPlayListFileExtensions();
    }

    bool PlayListFileScanner::needsScan(const FileToScan& file) const
    {
        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ _db.getTLSSession().createReadTransaction() };

        const db::PlayListFile::pointer playList{ db::PlayListFile::find(dbSession, file.filePath) };
        return !playList || playList->getLastWriteTime() != file.lastWriteTime;
    }

    std::unique_ptr<IFileScanOperation> PlayListFileScanner::createScanOperation(FileToScan&& fileToScan) const
    {
        return std::make_unique<PlayListFileScanOperation>(std::move(fileToScan), _db, _settings);
    }
} // namespace lms::scanner
