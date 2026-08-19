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

#include "Utils.hpp"

#include <cassert>
#include <system_error>

#include "core/ILogger.hpp"
#include "core/UUID.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Podcast.hpp"

namespace lms::podcast::utils
{
    std::filesystem::path getPodcastRelativePath(const db::Podcast::pointer& podcast)
    {
        assert(podcast);
        return podcast->getId().toString();
    }

    db::Artwork::pointer createArtworkFromImage(db::Session& session, const std::filesystem::path& filePath, const image::ImageProperties& probedImage)
    {
        std::error_code ec;
        const auto fileSize{ std::filesystem::file_size(filePath, ec) };
        if (ec)
        {
            LMS_LOG(PODCAST, ERROR, "Failed to get file size of " << filePath << ": " << ec.message());
            return db::Artwork::pointer{};
        }

        db::Image::pointer image{ session.create<db::Image>(filePath) };
        image.modify()->setFileSize(static_cast<std::size_t>(fileSize));
        if (probedImage.dimensions)
        {
            image.modify()->setWidth(probedImage.dimensions->width);
            image.modify()->setHeight(probedImage.dimensions->height);
        }
        image.modify()->setLastWriteTime(Wt::WDateTime::currentDateTime());
        image.modify()->setFormat(probedImage.format);

        return session.create<db::Artwork>(image);
    }

    std::string generateRandomFileName()
    {
        return core::UUID::generate().toString();
    }

    void removeFile(const std::filesystem::path& filePath)
    {
        std::error_code ec;
        std::filesystem::remove(filePath, ec);
        if (ec)
            LMS_LOG(PODCAST, WARNING, "Failed to remove file " << filePath << ": " << ec.message());
        else
            LMS_LOG(PODCAST, DEBUG, "Removed file " << filePath);
    }
} // namespace lms::podcast::utils