/*
 * Copyright (C) 2015 Emeric Poupon
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

#pragma once

#include <filesystem>
#include <vector>

#include "database/Types.hpp"
#include "image/IEncodedImage.hpp"
#include "services/artwork/IArtworkService.hpp"

#include "ImageCache.hpp"

namespace lms::db
{
    class Session;
}

namespace lms::av
{
    class IAudioFile;
}

namespace lms::cover
{
    class ArtworkService : public IArtworkService
    {
    public:
        ArtworkService(db::Db& db, const std::filesystem::path& defaultSvgCoverPath, const std::filesystem::path& defaultArtistImageSvgPath);

    private:
        ArtworkService(const ArtworkService&) = delete;
        ArtworkService& operator=(const ArtworkService&) = delete;

        std::shared_ptr<image::IEncodedImage> getTrackImage(db::TrackId trackId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage> getReleaseCover(db::ReleaseId releaseId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage> getArtistImage(db::ArtistId artistId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage> getDefaultReleaseCover() override;
        std::shared_ptr<image::IEncodedImage> getDefaultArtistImage() override;

        void flushCache() override;
        void setJpegQuality(unsigned quality) override;

        std::shared_ptr<image::IEncodedImage> getTrackImage(db::Session& dbSession, db::TrackId trackId, image::ImageSize width, bool allowReleaseFallback);
        std::unique_ptr<image::IEncodedImage> getFromAvMediaFile(const av::IAudioFile& input, image::ImageSize width) const;
        std::unique_ptr<image::IEncodedImage> getFromImageFile(const std::filesystem::path& p, image::ImageSize width) const;

        std::unique_ptr<image::IEncodedImage> getTrackImage(const std::filesystem::path& path, image::ImageSize width) const;

        bool checkImageFile(const std::filesystem::path& filePath) const;

        db::Db& _db;

        ImageCache _cache;
        std::shared_ptr<image::IEncodedImage> _defaultReleaseCover;
        std::shared_ptr<image::IEncodedImage> _defaultArtistImage;

        static inline const std::vector<std::filesystem::path> _fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" }; // TODO parametrize
        unsigned _jpegQuality;
    };

} // namespace lms::cover
