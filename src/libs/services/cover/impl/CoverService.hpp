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
#include <map>
#include <vector>

#include "services/cover/ICoverService.hpp"
#include "image/IEncodedImage.hpp"
#include "database/Types.hpp"
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
    class CoverService : public ICoverService
    {
    public:
        CoverService(db::Db& db, const std::filesystem::path& defaultSvgCoverPath);

    private:
        CoverService(const CoverService&) = delete;
        CoverService& operator=(const CoverService&) = delete;

        std::shared_ptr<image::IEncodedImage>   getFromTrack(db::TrackId trackId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage>   getFromRelease(db::ReleaseId releaseId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage>   getFromArtist(db::ArtistId artistId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage>   getDefaultSvgCover() override;
        void                                    flushCache() override;
        void                                    setJpegQuality(unsigned quality) override;

        std::shared_ptr<image::IEncodedImage>   getFromTrack(db::Session& dbSession, db::TrackId trackId, image::ImageSize width, bool allowReleaseFallback);
        std::unique_ptr<image::IEncodedImage>   getFromAvMediaFile(const av::IAudioFile& input, image::ImageSize width) const;
        std::unique_ptr<image::IEncodedImage>   getFromCoverFile(const std::filesystem::path& p, image::ImageSize width) const;

        std::unique_ptr<image::IEncodedImage>   getFromTrack(const std::filesystem::path& path, image::ImageSize width) const;
        std::multimap<std::string, std::filesystem::path>   getCoverPaths(const std::filesystem::path& directoryPath) const;
        std::unique_ptr<image::IEncodedImage>   getFromDirectory(const std::filesystem::path& directory, image::ImageSize width, const std::vector<std::string>& preferredFileNames, bool allowPickRandom) const;
        std::unique_ptr<image::IEncodedImage>   getFromSameNamedFile(const std::filesystem::path& filePath, image::ImageSize width) const;

        bool                                    checkCoverFile(const std::filesystem::path& filePath) const;

        db::Db& _db;

        ImageCache _cache;
        std::shared_ptr<image::IEncodedImage> _defaultCover;

        static inline const std::vector<std::filesystem::path> _fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" }; // TODO parametrize
        const std::size_t _maxFileSize;
        const std::vector<std::string> _preferredFileNames;
        const std::vector<std::string> _artistFileNames;
        unsigned _jpegQuality;
    };

} // namespace lms::cover

