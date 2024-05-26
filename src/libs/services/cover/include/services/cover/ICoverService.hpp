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
    class ICoverService
    {
    public:
        virtual ~ICoverService() = default;

        virtual std::shared_ptr<image::IEncodedImage> getFromTrack(db::TrackId trackId, image::ImageSize width) = 0;
        virtual std::shared_ptr<image::IEncodedImage> getFromRelease(db::ReleaseId releaseId, image::ImageSize width) = 0;
        virtual std::shared_ptr<image::IEncodedImage> getFromArtist(db::ArtistId artistId, image::ImageSize width) = 0;

        virtual std::shared_ptr<image::IEncodedImage> getDefaultSvgCover() = 0;

        virtual void flushCache() = 0;

        virtual void setJpegQuality(unsigned quality) = 0; // from 1 to 100
    };

    std::unique_ptr<ICoverService> createCoverService(db::Db& db, const std::filesystem::path& defaultSvgCoverPath);

} // namespace lms::cover
