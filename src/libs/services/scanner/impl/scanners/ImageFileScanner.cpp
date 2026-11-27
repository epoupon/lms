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

#include <array>
#include <optional>

#include "core/ILogger.hpp"

#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"
#include "services/scanner/ScanErrors.hpp"

#include "FileScanOperationBase.hpp"
#include "IFileScanOperation.hpp"
#include "Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        class ImageFileScanOperation : public FileScanOperationBase
        {
        public:
            using FileScanOperationBase::FileScanOperationBase;

        private:
            core::LiteralString getName() const override { return "ScanImageFile"; }
            void scan() override;
            OperationResult processResult() override;

            std::optional<image::ImageProperties> _parsedImage;
        };

        void ImageFileScanOperation::scan()
        {
            try
            {
                _parsedImage = image::probeImage(getFilePath());
            }
            catch (const image::IOFileException& e)
            {
                _parsedImage.reset();
                addError<IOScanError>(e.getPath(), e.getErrorCode());
            }
            catch (const image::Exception& e)
            {
                _parsedImage.reset();
                addError<ImageFileScanError>(getFilePath(), e.what());
            }
        }

        ImageFileScanOperation::OperationResult ImageFileScanOperation::processResult()
        {
            db::ISession& dbSession{ getDb().getTLSSession() };
            std::optional<db::Image> image{ db::Image::find(dbSession, getFilePath()) };

            if (!_parsedImage)
            {
                if (image)
                {
                    db::Image::destroy(dbSession, std::array{ image->getId() });

                    LMS_LOG(DBUPDATER, DEBUG, "Removed image " << getFilePath());
                    return OperationResult::Removed;
                }

                return OperationResult::Skipped;
            }

            const bool added{ !image };
            if (added)
            {
                image = db::Image::create(dbSession, getFilePath());
                db::Artwork::create(dbSession, image->getId());
            }
            else if (image->getLastWriteTime() != getLastWriteTime()
                     || image->getFileSize() != getFileSize())
            {
                if (const std::optional<db::Artwork> artwork{ db::Artwork::find(dbSession, image->getId()) })
                    db::Artwork::destroy(dbSession, std::array{ artwork->getId() });
                db::Artwork::create(dbSession, image->getId());
            }

            const image::ImageDimensions dimensions{ _parsedImage->dimensions.value_or(image::ImageDimensions{}) };
            image->setLastWriteTime(getLastWriteTime());
            image->setFileSize(getFileSize());
            image->setHeight(dimensions.height);
            image->setWidth(dimensions.width);
            image->setFormat(_parsedImage->format);
            std::optional<db::MediaLibrary> mediaLibrary{ db::MediaLibrary::find(dbSession, getMediaLibrary().id) }; // may be null if settings are updated in // => next scan will correct this
            image->setDirectory(utils::getOrCreateDirectory(dbSession, getFilePath().parent_path(), mediaLibrary).getId());
            db::Image::update(dbSession, *image);

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added image " << getFilePath());
                return OperationResult::Added;
            }

            LMS_LOG(DBUPDATER, DEBUG, "Updated image " << getFilePath());
            return OperationResult::Updated;
        }
    } // namespace

    ImageFileScanner::ImageFileScanner(db::IDb& db, ScannerSettings& settings)
        : _db{ db }
        , _settings{ settings }
    {
    }

    core::LiteralString ImageFileScanner::getName() const
    {
        return "Image scanner";
    }

    std::span<const std::filesystem::path> ImageFileScanner::getSupportedFiles() const
    {
        return {};
    }

    std::span<const std::filesystem::path> ImageFileScanner::getSupportedExtensions() const
    {
        return image::getSupportedFileExtensions();
    }

    bool ImageFileScanner::needsScan(const FileToScan& file) const
    {
        db::ISession& dbSession{ _db.getTLSSession() };
        auto transaction{ _db.getTLSSession().createReadTransaction() };

        const std::optional<db::Image> image{ db::Image::find(dbSession, file.filePath) };
        return (!image || image->getLastWriteTime() != file.lastWriteTime);
    }

    std::unique_ptr<IFileScanOperation> ImageFileScanner::createScanOperation(FileToScan&& fileToScan) const
    {
        return std::make_unique<ImageFileScanOperation>(std::move(fileToScan), _db, _settings);
    }
} // namespace lms::scanner
