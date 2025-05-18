/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "ArtistInfoFileScanner.hpp"

#include <fstream>

#include "core/ILogger.hpp"
#include "core/String.hpp"
#include "database/Artist.hpp"
#include "database/ArtistInfo.hpp"
#include "database/Db.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Session.hpp"
#include "metadata/ArtistInfo.hpp"

#include "IFileScanOperation.hpp"
#include "ScanContext.hpp"
#include "ScannerSettings.hpp"
#include "Utils.hpp"
#include "helpers/ArtistHelpers.hpp"
#include "metadata/Types.hpp"

namespace lms::scanner
{
    namespace
    {
        class ArtistInfoFileScanOperation : public IFileScanOperation
        {
        public:
            ArtistInfoFileScanOperation(const FileToScan& file, const ScannerSettings& settings, db::Db& db)
                : _file{ file.file }
                , _mediaLibrary{ file.mediaLibrary }
                , _settings{ settings }
                , _db{ db }
            {
            }
            ~ArtistInfoFileScanOperation() override = default;
            ArtistInfoFileScanOperation(const ArtistInfoFileScanOperation&) = delete;
            ArtistInfoFileScanOperation& operator=(const ArtistInfoFileScanOperation&) = delete;

        private:
            const std::filesystem::path& getFile() const override { return _file; };
            core::LiteralString getName() const override { return "ScanArtistInfoFile"; }
            void scan() override;
            void processResult(ScanContext& context) override;

            std::string getArtistNameFromArtistInfoFilePath();

            const std::filesystem::path _file;
            const MediaLibraryInfo _mediaLibrary;
            const ScannerSettings& _settings;
            db::Db& _db;

            std::optional<metadata::ArtistInfo> _parsedArtistInfo;
        };

        void ArtistInfoFileScanOperation::scan()
        {
            try
            {
                std::ifstream ifs{ _file };
                if (!ifs)
                {
                    LMS_LOG(DBUPDATER, ERROR, "Cannot open file " << _file);
                    return;
                }

                _parsedArtistInfo = metadata::parseArtistInfo(ifs);
                if (_parsedArtistInfo->name.empty())
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Discarding artist info in file " << _file << ": no name set");
                    _parsedArtistInfo.reset();
                }
            }
            catch (const metadata::ArtistInfoParseException& e)
            {
                LMS_LOG(DBUPDATER, ERROR, "Cannot read artist info in file " << _file << ": " << e.what());
            }
        }

        void ArtistInfoFileScanOperation::processResult(ScanContext& context)
        {
            ScanStats& stats{ context.stats };

            const std::optional<FileInfo> fileInfo{ utils::retrieveFileInfo(_file, _mediaLibrary.rootDirectory) };
            if (!fileInfo)
            {
                stats.skips++;
                return;
            }

            db::Session& dbSession{ _db.getTLSSession() };
            db::ArtistInfo::pointer artistInfo{ db::ArtistInfo::find(dbSession, _file) };
            if (!_parsedArtistInfo)
            {
                if (artistInfo)
                {
                    artistInfo.remove();
                    stats.deletions++;
                    LMS_LOG(DBUPDATER, DEBUG, "Removed artist info file " << _file);
                }
                context.stats.errors.emplace_back(_file, ScanErrorType::CannotReadArtistInfoFile);
                return;
            }

            const bool added{ !artistInfo };
            if (!artistInfo)
            {
                artistInfo = dbSession.create<db::ArtistInfo>();
                artistInfo.modify()->setAbsoluteFilePath(_file);
            }

            artistInfo.modify()->setScanVersion(_settings.artistInfoScanVersion);
            artistInfo.modify()->setName(_parsedArtistInfo->name);
            artistInfo.modify()->setSortName(_parsedArtistInfo->sortName);
            artistInfo.modify()->setLastWriteTime(fileInfo->lastWriteTime);
            artistInfo.modify()->setType(_parsedArtistInfo->type);
            artistInfo.modify()->setGender(_parsedArtistInfo->gender);
            artistInfo.modify()->setDisambiguation(_parsedArtistInfo->disambiguation);
            artistInfo.modify()->setBiography(_parsedArtistInfo->biography);

            db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, _mediaLibrary.id) }; // may be null if settings are updated in // => next scan will correct this
            artistInfo.modify()->setDirectory(utils::getOrCreateDirectory(dbSession, _file.parent_path(), mediaLibrary));

            const metadata::Artist artistMetadata{ _parsedArtistInfo->mbid, _parsedArtistInfo->name, _parsedArtistInfo->sortName.empty() ? std::nullopt : std::make_optional<std::string>(_parsedArtistInfo->sortName) };
            db::Artist::pointer artist{ helpers::getOrCreateArtist(dbSession, artistMetadata, helpers::AllowFallbackOnMBIDEntry{ _settings.allowArtistMBIDFallback }) };
            artistInfo.modify()->setArtist(artist);
            artistInfo.modify()->setMBIDMatched(_parsedArtistInfo->mbid.has_value() && _parsedArtistInfo->mbid == artist->getMBID());

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added artist info file " << _file);
                stats.additions++;
            }
            else
            {
                LMS_LOG(DBUPDATER, DEBUG, "Updated artist info file " << _file);
                stats.updates++;
            }
        }
    } // namespace

    ArtistInfoFileScanner::ArtistInfoFileScanner(const ScannerSettings& settings, db::Db& db)
        : _settings{ settings }
        , _db{ db }
    {
    }

    core::LiteralString ArtistInfoFileScanner::getName() const
    {
        return "Artist info scanner";
    }

    std::span<const std::filesystem::path> ArtistInfoFileScanner::getSupportedExtensions() const
    {
        return metadata::getSupportedInfoFileExtensions();
    }

    bool ArtistInfoFileScanner::needsScan(ScanContext& context, const FileToScan& file) const
    {
        // Special case: only files named "artist.nfo" are compatible with this scanner
        // Hack here since the scanner framework only handle extensions (the discover count is not accurate)
        if (!core::stringUtils::stringCaseInsensitiveEqual(file.file.stem().string(), "artist"))
            return false;

        const Wt::WDateTime lastWriteTime{ utils::retrieveFileGetLastWrite(file.file) };
        // Should rarely fail as we are currently iterating it
        if (!lastWriteTime.isValid())
        {
            context.stats.skips++;
            return false;
        }

        if (context.scanOptions.fullScan)
            return true;

        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };
        db::ArtistInfo::pointer artistInfo{ db::ArtistInfo::find(dbSession, file.file) };
        if (artistInfo
            && artistInfo->getLastWriteTime() == lastWriteTime
            && artistInfo->getScanVersion() == _settings.artistInfoScanVersion)
        {
            context.stats.skips++;
            return false;
        }

        return true;
    }

    std::unique_ptr<IFileScanOperation> ArtistInfoFileScanner::createScanOperation(const FileToScan& fileToScan) const
    {
        return std::make_unique<ArtistInfoFileScanOperation>(fileToScan, _settings, _db);
    }
} // namespace lms::scanner