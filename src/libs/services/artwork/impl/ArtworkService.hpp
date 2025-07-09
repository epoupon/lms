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

#include "database/objects/ImageId.hpp"
#include "database/objects/TrackEmbeddedImageId.hpp"
#include "services/artwork/IArtworkService.hpp"

#include "ImageCache.hpp"

namespace lms::db
{
    class Session;
}

namespace lms::metadata
{
    class IAudioFileParser;
}

namespace lms::artwork
{
    class ArtworkService : public IArtworkService
    {
    public:
        ArtworkService(db::IDb& db, const std::filesystem::path& defaultReleaseCoverSvgPath, const std::filesystem::path& defaultArtistImageSvgPath);
        ~ArtworkService() override;
        ArtworkService(const ArtworkService&) = delete;
        ArtworkService& operator=(const ArtworkService&) = delete;

    private:
        db::ArtworkId findTrackListImage(db::TrackListId trackListId) override;

        std::shared_ptr<image::IEncodedImage> getImage(db::ArtworkId artworkId, std::optional<image::ImageSize> width) override;

        std::shared_ptr<image::IEncodedImage> getDefaultReleaseArtwork() override;
        std::shared_ptr<image::IEncodedImage> getDefaultArtistArtwork() override;

        void flushCache() override;
        void setJpegQuality(unsigned quality) override;

        std::shared_ptr<image::IEncodedImage> getImage(db::ImageId imageId, std::optional<image::ImageSize> width);
        std::shared_ptr<image::IEncodedImage> getTrackEmbeddedImage(db::TrackEmbeddedImageId trackEmbeddedImageId, std::optional<image::ImageSize> width);

        std::unique_ptr<image::IEncodedImage> getFromImageFile(const std::filesystem::path& p, std::optional<image::ImageSize> width) const;
        std::unique_ptr<image::IEncodedImage> getTrackImage(const std::filesystem::path& path, std::size_t index, std::optional<image::ImageSize> width) const;

        db::IDb& _db;

        std::unique_ptr<metadata::IAudioFileParser> _audioFileParser;
        ImageCache _cache;
        std::shared_ptr<image::IEncodedImage> _defaultReleaseCover;
        std::shared_ptr<image::IEncodedImage> _defaultArtistImage;

        static inline const std::vector<std::filesystem::path> _fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" }; // TODO parametrize
        unsigned _jpegQuality;
    };

} // namespace lms::artwork
