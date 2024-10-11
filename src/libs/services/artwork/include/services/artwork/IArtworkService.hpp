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

#include "database/ArtistId.hpp"
#include "database/ReleaseId.hpp"
#include "database/TrackId.hpp"
#include "image/IEncodedImage.hpp"

namespace lms::db
{
    class Db;
}

namespace lms::cover
{
    class IArtworkService
    {
    public:
        virtual ~IArtworkService() = default;

        virtual std::shared_ptr<image::IEncodedImage> getArtistImage(db::ArtistId artistId, image::ImageSize width) = 0;

        // no logic to fallback to release here
        virtual std::shared_ptr<image::IEncodedImage> getTrackImage(db::TrackId trackId, image::ImageSize width) = 0;

        // no logic to fallback to track here
        virtual std::shared_ptr<image::IEncodedImage> getReleaseCover(db::ReleaseId releaseId, image::ImageSize width) = 0;

        // Svg images dont have image "size"
        virtual std::shared_ptr<image::IEncodedImage> getDefaultReleaseCover() = 0;
        virtual std::shared_ptr<image::IEncodedImage> getDefaultArtistImage() = 0;

        virtual void flushCache() = 0;

        virtual void setJpegQuality(unsigned quality) = 0; // from 1 to 100
    };

    std::unique_ptr<IArtworkService> createArtworkService(db::Db& db, const std::filesystem::path& defaultSvgCoverPath, const std::filesystem::path& defaultArtistImageSvgPath);

} // namespace lms::cover
