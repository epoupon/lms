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

#include <atomic>
#include <filesystem>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "services/cover/ICoverService.hpp"
#include "image/IEncodedImage.hpp"
#include "database/Types.hpp"

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
    struct CacheEntryDesc
    {
        std::variant<db::ArtistId, db::ReleaseId, db::TrackId> id;
        std::size_t			size;

        bool operator==(const CacheEntryDesc& other) const
        {
            return id == other.id
                && size == other.size;
        }
    };
} // ns Cover

namespace std
{
    template<>
    class hash<lms::cover::CacheEntryDesc>
    {
    public:
        size_t operator()(const lms::cover::CacheEntryDesc& e) const
        {
            size_t h{};
            std::visit([&](auto id)
                {
                    using IdType = std::decay_t<decltype(id)>;
                    h ^= std::hash<IdType>{}(id);
                }, e.id);
            h ^= std::hash<std::size_t>{}(e.size) << 1;
            return h;
        }
    };

} // ns std

namespace lms::cover
{
    class CoverService : public ICoverService
    {
    public:
        CoverService(db::Db& db, const std::filesystem::path& execPath, const std::filesystem::path& defaultCoverPath);

        CoverService(const CoverService&) = delete;
        CoverService& operator=(const CoverService&) = delete;

    private:
        std::shared_ptr<image::IEncodedImage>   getFromTrack(db::TrackId trackId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage>   getFromRelease(db::ReleaseId releaseId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage>   getFromArtist(db::ArtistId artistId, image::ImageSize width) override;
        std::shared_ptr<image::IEncodedImage>   getDefault(image::ImageSize width) override;
        void                                    flushCache() override;
        void                                    setJpegQuality(unsigned quality) override;

        std::shared_ptr<image::IEncodedImage>   getFromTrack(db::Session& dbSession, db::TrackId trackId, image::ImageSize width, bool allowReleaseFallback);
        std::unique_ptr<image::IEncodedImage>   getFromAvMediaFile(const av::IAudioFile& input, image::ImageSize width) const;
        std::unique_ptr<image::IEncodedImage>   getFromCoverFile(const std::filesystem::path& p, image::ImageSize width) const;

        std::unique_ptr<image::IEncodedImage>   getFromTrack(const std::filesystem::path& path, image::ImageSize width) const;
        std::multimap<std::string, std::filesystem::path>   getCoverPaths(const std::filesystem::path& directoryPath) const;
        std::unique_ptr<image::IEncodedImage>   getFromDirectory(const std::filesystem::path& directory, image::ImageSize width, const std::vector<std::string>& preferredFileNames, bool allowPickRandom) const;
        std::unique_ptr<image::IEncodedImage>   getFromSameNamedFile(const std::filesystem::path& filePath, image::ImageSize width) const;

        bool                                    checkCoverFile(const std::filesystem::path& directoryPath) const;

        db::Db& _db;

        std::shared_mutex _cacheMutex;
        std::unordered_map<CacheEntryDesc, std::shared_ptr<image::IEncodedImage>> _cache;
        std::unordered_map<image::ImageSize, std::shared_ptr<image::IEncodedImage>> _defaultCoverCache;
        std::atomic<std::size_t>    _cacheMisses{};
        std::atomic<std::size_t>    _cacheHits{};
        std::size_t                 _cacheSize{};

        void saveToCache(const CacheEntryDesc& entryDesc, std::shared_ptr<image::IEncodedImage> image);
        std::shared_ptr<image::IEncodedImage> loadFromCache(const CacheEntryDesc& entryDesc);

        const std::filesystem::path _defaultCoverPath;
        const std::size_t _maxCacheSize;
        static inline const std::vector<std::filesystem::path> _fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" }; // TODO parametrize
        const std::size_t _maxFileSize;
        const std::vector<std::string> _preferredFileNames;
        const std::vector<std::string> _artistFileNames;
        unsigned _jpegQuality;
    };

} // namespace lms::cover

