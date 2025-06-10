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
#include <memory>
#include <optional>
#include <variant>

#include "database/ArtistId.hpp"
#include "database/ImageId.hpp"
#include "database/ReleaseId.hpp"
#include "database/TrackEmbeddedImageId.hpp"
#include "database/TrackId.hpp"
#include "database/TrackListId.hpp"
#include "image/IEncodedImage.hpp"

namespace lms::db
{
    class Db;
}

namespace lms::artwork
{
    class IArtworkService
    {
    public:
        virtual ~IArtworkService() = default;

        // Helpers to find artworks
        using ImageFindResult = std::variant<std::monostate, db::ImageId, db::TrackEmbeddedImageId>;
        virtual ImageFindResult findArtistImage(db::ArtistId artistId) = 0;

        // Will get Disc/Media artwork if available, otherwise, will fallback on release artwork
        virtual ImageFindResult findTrackImage(db::TrackId trackId) = 0;

        // Will get Disc/Media artwork if available, no fallback
        virtual ImageFindResult findTrackMediaImage(db::TrackId trackId) = 0;

        // Will get Release if available, otherwise, will fallback on embedded artworks
        virtual ImageFindResult findReleaseImage(db::ReleaseId releaseId) = 0;
        virtual ImageFindResult findTrackListImage(db::TrackListId trackListId) = 0;

        // Image retrieval
        virtual std::shared_ptr<image::IEncodedImage> getImage(db::ImageId imageId, std::optional<image::ImageSize> width) = 0;
        virtual std::shared_ptr<image::IEncodedImage> getTrackEmbeddedImage(db::TrackEmbeddedImageId trackEmbeddedImageId, std::optional<image::ImageSize> width) = 0;

        // Svg images dont have image "size"
        virtual std::shared_ptr<image::IEncodedImage> getDefaultReleaseCover() = 0;
        virtual std::shared_ptr<image::IEncodedImage> getDefaultArtistImage() = 0;

        virtual void flushCache() = 0;

        virtual void setJpegQuality(unsigned quality) = 0; // from 1 to 100
    };

    std::unique_ptr<IArtworkService> createArtworkService(db::Db& db, const std::filesystem::path& defaultReleaseCoverSvgPath, const std::filesystem::path& defaultArtistImageSvgPath);

} // namespace lms::artwork
