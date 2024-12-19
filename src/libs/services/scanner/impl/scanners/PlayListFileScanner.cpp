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
#include "database/Db.hpp"
#include "database/MediaLibrary.hpp"
#include "database/PlayListFile.hpp"
#include "database/Session.hpp"
#include "metadata/Exception.hpp"
#include "metadata/PlayList.hpp"

#include "IFileScanOperation.hpp"
#include "ScanContext.hpp"
#include "Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        class PlayListFileScanOperation : public IFileScanOperation
        {
        public:
            PlayListFileScanOperation(const FileToScan& file, db::Db& db)
                : _file{ file.file }
                , _mediaLibrary{ file.mediaLibrary }
                , _db{ db } {}
            ~PlayListFileScanOperation() override = default;
            PlayListFileScanOperation(const PlayListFileScanOperation&) = delete;
            PlayListFileScanOperation& operator=(const PlayListFileScanOperation&) = delete;

        private:
            const std::filesystem::path& getFile() const override { return _file; };
            core::LiteralString getName() const override { return "ScanPlayListFile"; }
            void scan() override;
            void processResult(ScanContext& context) override;

            const std::filesystem::path _file;
            const MediaLibraryInfo _mediaLibrary;
            db::Db& _db;

            std::optional<metadata::PlayList> _parsedPlayList;
        };

        void PlayListFileScanOperation::scan()
        {
            try
            {
                std::ifstream ifs{ _file.string() };
                if (!ifs)
                    LMS_LOG(DBUPDATER, ERROR, "Cannot open file '" << _file.string() << "'");
                else
                    _parsedPlayList = metadata::parsePlayList(ifs);
            }
            catch (const metadata::Exception& e)
            {
                LMS_LOG(DBUPDATER, ERROR, "Cannot read playlist in file '" << _file.string() << "': " << e.what());
            }
        }

        void PlayListFileScanOperation::processResult(ScanContext& context)
        {
            ScanStats& stats{ context.stats };

            const std::optional<FileInfo> fileInfo{ utils::retrieveFileInfo(_file, _mediaLibrary.rootDirectory) };
            if (!fileInfo)
            {
                stats.skips++;
                return;
            }

            db::Session& dbSession{ _db.getTLSSession() };
            db::PlayListFile::pointer playList{ db::PlayListFile::find(dbSession, _file) };

            if (!_parsedPlayList)
            {
                if (playList)
                {
                    playList.remove();
                    stats.deletions++;
                }
                context.stats.errors.emplace_back(_file, ScanErrorType::CannotReadPlayListFile);
                return;
            }

            const bool added{ !playList };
            if (!playList)
                playList = dbSession.create<db::PlayListFile>(_file);

            playList.modify()->setLastWriteTime(fileInfo->lastWriteTime);
            playList.modify()->setFileSize(fileInfo->fileSize);
            if (!_parsedPlayList->name.empty())
                playList.modify()->setName(_parsedPlayList->name);
            else
                playList.modify()->setName(_file.stem().string());
            playList.modify()->setFiles(_parsedPlayList->files);

            db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, _mediaLibrary.id) }; // may be null if settings are updated in // => next scan will correct this
            playList.modify()->setDirectory(utils::getOrCreateDirectory(dbSession, _file.parent_path(), mediaLibrary));

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added playlist file '" << _file.string() << "'");
                LMS_LOG(DBUPDATER, DEBUG, "db playlist file = '" << playList->getAbsoluteFilePath().string() << "'");
                stats.additions++;
            }
            else
            {
                LMS_LOG(DBUPDATER, DEBUG, "Updated playlist file '" << _file.string() << "'");
                stats.updates++;
            }
        }
    } // namespace

    PlayListFileScanner::PlayListFileScanner(db::Db& db)
        : _db{ db }
    {
    }

    core::LiteralString PlayListFileScanner::getName() const
    {
        return "PlayList scanner";
    }

    std::span<const std::filesystem::path> PlayListFileScanner::getSupportedExtensions() const
    {
        return metadata::getSupportedPlayListFileExtensions();
    }

    bool PlayListFileScanner::needsScan(ScanContext& context, const FileToScan& file) const
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

            const db::PlayListFile::pointer playList{ db::PlayListFile::find(dbSession, file.file) };
            if (playList && playList->getLastWriteTime() == lastWriteTime)
            {
                stats.skips++;
                return false;
            }
        }

        return true; // need to scan
    }

    std::unique_ptr<IFileScanOperation> PlayListFileScanner::createScanOperation(const FileToScan& fileToScan) const
    {
        return std::make_unique<PlayListFileScanOperation>(fileToScan, _db);
    }
} // namespace lms::scanner
