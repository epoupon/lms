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

#include "ImageFileScanner.hpp"

#include <optional>

#include "core/ILogger.hpp"
#include "database/Db.hpp"
#include "database/Image.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Session.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"

#include "IFileScanOperation.hpp"
#include "ScanContext.hpp"
#include "Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        class ImageFileScanOperation : public IFileScanOperation
        {
        public:
            ImageFileScanOperation(const FileToScan& file, db::Db& db)
                : _file{ file.file }
                , _mediaLibrary{ file.mediaLibrary }
                , _db{ db } {}

        private:
            const std::filesystem::path& getFile() const override { return _file; };
            core::LiteralString getName() const override { return "ScanImageFile"; }
            void scan() override;
            void processResult(ScanContext& context) override;

            const std::filesystem::path _file;
            const MediaLibraryInfo _mediaLibrary;
            db::Db& _db;

            struct ImageInfo
            {
                std::size_t height{};
                std::size_t width{};
            };
            std::optional<ImageInfo> _parsedImageInfo;
        };

        void ImageFileScanOperation::scan()
        {
            try
            {
                std::unique_ptr<image::IRawImage> rawImage{ image::decodeImage(_file) };

                ImageInfo imageInfo;
                imageInfo.width = rawImage->getWidth();
                imageInfo.height = rawImage->getHeight();

                _parsedImageInfo = imageInfo;
            }
            catch (const image::Exception& e)
            {
                LMS_LOG(DBUPDATER, ERROR, "Cannot read image in file '" << _file.string() << "': " << e.what());
            }
        }

        void ImageFileScanOperation::processResult(ScanContext& context)
        {
            ScanStats& stats{ context.stats };

            const std::optional<FileInfo> fileInfo{ utils::retrieveFileInfo(_file, _mediaLibrary.rootDirectory) };
            if (!fileInfo)
            {
                stats.skips++;
                return;
            }

            db::Session& dbSession{ _db.getTLSSession() };
            db::Image::pointer image{ db::Image::find(dbSession, _file) };

            if (!_parsedImageInfo)
            {
                if (image)
                {
                    image.remove();
                    stats.deletions++;
                }
                context.stats.errors.emplace_back(_file, ScanErrorType::CannotReadImageFile);
                return;
            }

            const bool added{ !image };
            if (!image)
                image = dbSession.create<db::Image>(_file);

            image.modify()->setLastWriteTime(fileInfo->lastWriteTime);
            image.modify()->setFileSize(fileInfo->fileSize);
            image.modify()->setHeight(_parsedImageInfo->height);
            image.modify()->setWidth(_parsedImageInfo->width);
            db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, _mediaLibrary.id) }; // may be null if settings are updated in // => next scan will correct this
            image.modify()->setDirectory(utils::getOrCreateDirectory(dbSession, _file.parent_path(), mediaLibrary));

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added image '" << _file.string() << "'");
                stats.additions++;
            }
            else
            {
                LMS_LOG(DBUPDATER, DEBUG, "Updated image '" << _file.string() << "'");
                stats.updates++;
            }
        }
    } // namespace

    ImageFileScanner::ImageFileScanner(db::Db& db)
        : _db{ db }
    {
    }

    core::LiteralString ImageFileScanner::getName() const
    {
        return "Image scanner";
    }

    std::span<const std::filesystem::path> ImageFileScanner::getSupportedExtensions() const
    {
        return image::getSupportedFileExtensions();
    }

    bool ImageFileScanner::needsScan(ScanContext& context, const FileToScan& file) const
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

            const db::Image::pointer image{ db::Image::find(dbSession, file.file) };
            if (image && image->getLastWriteTime() == lastWriteTime)
            {
                stats.skips++;
                return false;
            }
        }

        return true; // need to scan
    }

    std::unique_ptr<IFileScanOperation> ImageFileScanner::createScanOperation(const FileToScan& fileToScan) const
    {
        return std::make_unique<ImageFileScanOperation>(fileToScan, _db);
    }
} // namespace lms::scanner
