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
#include <system_error>

#include "core/ILogger.hpp"
#include "core/String.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "metadata/ArtistInfo.hpp"
#include "metadata/Types.hpp"
#include "services/scanner/ScanErrors.hpp"

#include "FileScanOperationBase.hpp"
#include "ScannerSettings.hpp"
#include "Utils.hpp"
#include "helpers/ArtistHelpers.hpp"

namespace lms::scanner
{
    namespace
    {
        class ArtistInfoFileScanOperation : public FileScanOperationBase
        {
        public:
            using FileScanOperationBase::FileScanOperationBase;
            ~ArtistInfoFileScanOperation() override = default;
            ArtistInfoFileScanOperation(const ArtistInfoFileScanOperation&) = delete;
            ArtistInfoFileScanOperation& operator=(const ArtistInfoFileScanOperation&) = delete;

        private:
            core::LiteralString getName() const override { return "ScanArtistInfoFile"; }
            void scan() override;
            OperationResult processResult() override;

            std::string getArtistNameFromArtistInfoFilePath();

            std::optional<metadata::ArtistInfo> _parsedArtistInfo;
        };

        void ArtistInfoFileScanOperation::scan()
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

                _parsedArtistInfo = metadata::parseArtistInfo(ifs);
                if (_parsedArtistInfo->name.empty())
                {
                    _parsedArtistInfo->name = getFilePath().parent_path().filename();
                    LMS_LOG(DBUPDATER, DEBUG, "No name found in " << getFilePath() << ", using '" << _parsedArtistInfo->name << "'");
                }
            }
            catch (const metadata::ArtistInfoParseException& e)
            {
                addError<ArtistInfoFileScanError>(getFilePath());
            }
        }

        ArtistInfoFileScanOperation::OperationResult ArtistInfoFileScanOperation::processResult()
        {
            db::Session& dbSession{ getDb().getTLSSession() };
            db::ArtistInfo::pointer artistInfo{ db::ArtistInfo::find(dbSession, getFilePath()) };
            if (!_parsedArtistInfo)
            {
                if (artistInfo)
                {
                    artistInfo.remove();

                    LMS_LOG(DBUPDATER, DEBUG, "Removed artist info file " << getFilePath());
                    return OperationResult::Removed;
                }

                return OperationResult::Skipped;
            }

            const bool added{ !artistInfo };
            if (!artistInfo)
            {
                artistInfo = dbSession.create<db::ArtistInfo>();
                artistInfo.modify()->setAbsoluteFilePath(getFilePath());
            }

            artistInfo.modify()->setScanVersion(getScannerSettings().artistInfoScanVersion);
            artistInfo.modify()->setName(_parsedArtistInfo->name);
            artistInfo.modify()->setSortName(_parsedArtistInfo->sortName);
            artistInfo.modify()->setLastWriteTime(getLastWriteTime());
            artistInfo.modify()->setType(_parsedArtistInfo->type);
            artistInfo.modify()->setGender(_parsedArtistInfo->gender);
            artistInfo.modify()->setDisambiguation(_parsedArtistInfo->disambiguation);
            artistInfo.modify()->setBiography(_parsedArtistInfo->biography);

            db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, getMediaLibrary().id) }; // may be null if settings are updated in // => next scan will correct this
            artistInfo.modify()->setDirectory(utils::getOrCreateDirectory(dbSession, getFilePath().parent_path(), mediaLibrary));

            const metadata::Artist artistMetadata{ _parsedArtistInfo->mbid, _parsedArtistInfo->name, _parsedArtistInfo->sortName.empty() ? std::nullopt : std::make_optional<std::string>(_parsedArtistInfo->sortName) };
            db::Artist::pointer artist{ helpers::getOrCreateArtist(dbSession, artistMetadata, helpers::AllowFallbackOnMBIDEntry{ getScannerSettings().allowArtistMBIDFallback }) };
            artistInfo.modify()->setArtist(artist);
            artistInfo.modify()->setMBIDMatched(_parsedArtistInfo->mbid.has_value() && _parsedArtistInfo->mbid == artist->getMBID());

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added artist info file " << getFilePath());
                return OperationResult::Added;
            }

            LMS_LOG(DBUPDATER, DEBUG, "Updated artist info file " << getFilePath());
            return OperationResult::Updated;
        }
    } // namespace

    ArtistInfoFileScanner::ArtistInfoFileScanner(db::IDb& db, const ScannerSettings& settings)
        : _db{ db }
        , _settings{ settings }
    {
    }

    core::LiteralString ArtistInfoFileScanner::getName() const
    {
        return "Artist info scanner";
    }

    std::span<const std::filesystem::path> ArtistInfoFileScanner::getSupportedFiles() const
    {
        return metadata::getSupportedArtistInfoFiles();
    }

    std::span<const std::filesystem::path> ArtistInfoFileScanner::getSupportedExtensions() const
    {
        return {};
    }

    bool ArtistInfoFileScanner::needsScan(const FileToScan& file) const
    {
        // Special case: only files named "artist.nfo" are compatible with this scanner
        // Hack here since the scanner framework only handle extensions (the discover count is not accurate)
        if (!core::stringUtils::stringCaseInsensitiveEqual(file.filePath.stem().string(), "artist"))
            return false;

        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };
        db::ArtistInfo::pointer artistInfo{ db::ArtistInfo::find(dbSession, file.filePath) };

        return !artistInfo
            || artistInfo->getLastWriteTime() != file.lastWriteTime
            || artistInfo->getScanVersion() != _settings.artistInfoScanVersion;
    }

    std::unique_ptr<IFileScanOperation> ArtistInfoFileScanner::createScanOperation(FileToScan&& fileToScan) const
    {
        return std::make_unique<ArtistInfoFileScanOperation>(std::move(fileToScan), _db, _settings);
    }
} // namespace lms::scanner