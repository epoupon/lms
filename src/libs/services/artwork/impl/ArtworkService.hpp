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
        ArtworkService(db::Db& db, const std::filesystem::path& defaultReleaseCoverSvgPath, const std::filesystem::path& defaultArtistImageSvgPath);
        ~ArtworkService() override = default;
        ArtworkService(const ArtworkService&) = delete;
        ArtworkService& operator=(const ArtworkService&) = delete;

    private:
        std::shared_ptr<image::IEncodedImage> getImage(db::ImageId imageId, std::optional<image::ImageSize> width) override;
        std::shared_ptr<image::IEncodedImage> getTrackImage(db::TrackId trackId, std::optional<image::ImageSize> width) override;
        std::shared_ptr<image::IEncodedImage> getDefaultReleaseCover() override;
        std::shared_ptr<image::IEncodedImage> getDefaultArtistImage() override;

        void flushCache() override;
        void setJpegQuality(unsigned quality) override;

        std::unique_ptr<image::IEncodedImage> getFromAvMediaFile(const av::IAudioFile& input, std::optional<image::ImageSize> width) const;
        std::unique_ptr<image::IEncodedImage> getFromImageFile(const std::filesystem::path& p, std::optional<image::ImageSize> width) const;
        std::unique_ptr<image::IEncodedImage> getTrackImage(const std::filesystem::path& path, std::optional<image::ImageSize> width) const;

        static bool checkImageFile(const std::filesystem::path& filePath);

        db::Db& _db;

        ImageCache _cache;
        std::shared_ptr<image::IEncodedImage> _defaultReleaseCover;
        std::shared_ptr<image::IEncodedImage> _defaultArtistImage;

        static inline const std::vector<std::filesystem::path> _fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" }; // TODO parametrize
        unsigned _jpegQuality;
    };

} // namespace lms::cover
