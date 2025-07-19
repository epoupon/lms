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

#include "database/objects/ArtworkId.hpp"
#include "database/objects/TrackListId.hpp"
#include "image/IEncodedImage.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::artwork
{
    class IArtworkService
    {
    public:
        virtual ~IArtworkService() = default;

        // Helpers to get preferred artworks
        virtual db::ArtworkId findTrackListImage(db::TrackListId trackListId) = 0;

        // Image retrieval, no width means original size
        virtual std::shared_ptr<image::IEncodedImage> getImage(db::ArtworkId artworkId, std::optional<image::ImageSize> width) = 0;

        // Svg images don't have image "size"
        virtual std::shared_ptr<image::IEncodedImage> getDefaultReleaseArtwork() = 0;
        virtual std::shared_ptr<image::IEncodedImage> getDefaultArtistArtwork() = 0;

        virtual void flushCache() = 0;

        virtual void setJpegQuality(unsigned quality) = 0; // from 1 to 100
    };

    std::unique_ptr<IArtworkService> createArtworkService(db::IDb& db, const std::filesystem::path& defaultReleaseCoverSvgPath, const std::filesystem::path& defaultArtistImageSvgPath);

} // namespace lms::artwork
