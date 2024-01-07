/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "CoverService.hpp"

#include <set>

#include "av/IAudioFile.hpp"

#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "image/Exception.hpp"
#include "image/IRawImage.hpp"
#include "utils/IConfig.hpp"
#include "utils/ILogger.hpp"
#include "utils/Path.hpp"
#include "utils/Random.hpp"
#include "utils/String.hpp"
#include "utils/Utils.hpp"

namespace Cover
{
    namespace
    {
        struct TrackInfo
        {
            bool hasCover{};
            bool isMultiDisc{};
            std::filesystem::path trackPath;
            std::optional<Database::ReleaseId> releaseId;
        };

        std::optional<TrackInfo> getTrackInfo(Database::Session& dbSession, Database::TrackId trackId)
        {
            std::optional<TrackInfo> res;

            auto transaction{ dbSession.createReadTransaction() };

            const Database::Track::pointer track{ Database::Track::find(dbSession, trackId) };
            if (!track)
                return res;

            res = TrackInfo{};

            res->hasCover = track->hasCover();
            res->trackPath = track->getPath();

            if (const Database::Release::pointer & release{ track->getRelease() })
            {
                res->releaseId = release->getId();
                if (release->getTotalDisc() > 1)
                    res->isMultiDisc = true;
            }

            return res;
        }

        std::vector<std::string> constructPreferredFileNames()
        {
            std::vector<std::string> res;

            Service<IConfig>::get()->visitStrings("cover-preferred-file-names",
                [&res](std::string_view fileName)
                {
                    res.emplace_back(fileName);
                }, { "cover", "front" });

            return res;
        }

        std::vector<std::string> constructArtistFileNames()
        {
            std::vector<std::string> res;

            Service<IConfig>::get()->visitStrings("artist-image-file-names",
                [&res](std::string_view fileName)
                {
                    res.emplace_back(fileName);
                }, { "artist" });

            return res;
        }

        bool isFileSupported(const std::filesystem::path& file, const std::vector<std::filesystem::path>& extensions)
        {
            return (std::find(std::cbegin(extensions), std::cend(extensions), file.extension()) != std::cend(extensions));
        }
    }

    std::unique_ptr<ICoverService> createCoverService(Database::Db& db, const std::filesystem::path& execPath, const std::filesystem::path& defaultCoverPath)
    {
        return std::make_unique<CoverService>(db, execPath, defaultCoverPath);
    }

    using namespace Image;

    CoverService::CoverService(Database::Db& db,
        const std::filesystem::path& execPath,
        const std::filesystem::path& defaultCoverPath)
        : _db{ db }
        , _defaultCoverPath{ defaultCoverPath }
        , _maxCacheSize{ Service<IConfig>::get()->getULong("cover-max-cache-size", 30) * 1000 * 1000 }
        , _maxFileSize{ Service<IConfig>::get()->getULong("cover-max-file-size", 10) * 1000 * 1000 }
        , _preferredFileNames{ constructPreferredFileNames() }
        , _artistFileNames{ constructArtistFileNames() }
    {
        setJpegQuality(Service<IConfig>::get()->getULong("cover-jpeg-quality", 75));

        LMS_LOG(COVER, INFO, "Default cover path = '" << _defaultCoverPath.string() << "'");
        LMS_LOG(COVER, INFO, "Max cache size = " << _maxCacheSize);
        LMS_LOG(COVER, INFO, "Max file size = " << _maxFileSize);
        LMS_LOG(COVER, INFO, "Preferred file names: " << StringUtils::joinStrings(_preferredFileNames, ","));

#if LMS_SUPPORT_IMAGE_GM
        GraphicsMagick::init(execPath);
#else
        (void)execPath;
#endif

        try
        {
            getDefault(512);
        }
        catch (const Image::ImageException& e)
        {
            throw LmsException("Cannot read default cover file '" + _defaultCoverPath.string() + "': " + e.what());
        }
    }

    std::unique_ptr<IEncodedImage> CoverService::getFromAvMediaFile(const Av::IAudioFile& input, ImageSize width) const
    {
        std::unique_ptr<IEncodedImage> image;

        input.visitAttachedPictures([&](const Av::Picture& picture)
            {
                if (image)
                    return;

                try
                {
                    std::unique_ptr<IRawImage> rawImage{ decodeImage(picture.data, picture.dataSize) };
                    rawImage->resize(width);
                    image = rawImage->encodeToJPEG(_jpegQuality);
                }
                catch (const Image::ImageException& e)
                {
                    LMS_LOG(COVER, ERROR, "Cannot read embedded cover: " << e.what());
                }
            });

        return image;
    }

    std::unique_ptr<IEncodedImage> CoverService::getFromCoverFile(const std::filesystem::path& p, ImageSize width) const
    {
        std::unique_ptr<IEncodedImage> image;

        try
        {
            std::unique_ptr<IRawImage> rawImage{ decodeImage(p) };
            rawImage->resize(width);
            image = rawImage->encodeToJPEG(_jpegQuality);
        }
        catch (const ImageException& e)
        {
            LMS_LOG(COVER, ERROR, "Cannot read cover in file '" << p.string() << "': " << e.what());
        }

        return image;
    }

    std::shared_ptr<IEncodedImage> CoverService::getDefault(ImageSize width)
    {
        {
            std::shared_lock lock{ _cacheMutex };

            if (auto it{ _defaultCoverCache.find(width) }; it != std::cend(_defaultCoverCache))
                return it->second;
        }

        {
            std::unique_lock lock{ _cacheMutex };

            if (auto it{ _defaultCoverCache.find(width) }; it != std::cend(_defaultCoverCache))
                return it->second;

            std::shared_ptr<IEncodedImage> image{ getFromCoverFile(_defaultCoverPath, width) };
            _defaultCoverCache[width] = image;
            LMS_LOG(COVER, DEBUG, "Default cache entries = " << _defaultCoverCache.size());

            return image;
        }
    }

    std::unique_ptr<IEncodedImage> CoverService::getFromDirectory(const std::filesystem::path& directory, ImageSize width, const std::vector<std::string>& preferredFileNames, bool allowPickRandom) const
    {
        const std::multimap<std::string, std::filesystem::path> coverPaths{ getCoverPaths(directory) };

        auto tryLoadImageFromFilename = [&](std::string_view fileName)
            {
                std::unique_ptr<IEncodedImage> image;

                auto range{ coverPaths.equal_range(std::string {fileName}) };
                for (auto it{ range.first }; it != range.second; ++it)
                {
                    image = getFromCoverFile(it->second, width);
                    if (image)
                        break;
                }
                return image;
            };

        std::unique_ptr<IEncodedImage> image;

        for (std::string_view filename : preferredFileNames)
        {
            image = tryLoadImageFromFilename(filename);
            if (image)
                return image;
        }

        if (allowPickRandom)
        {
            for (const auto& [filename, coverPath] : coverPaths)
            {
                image = getFromCoverFile(coverPath, width);
                if (image)
                    return image;
            }
        }

        return image;
    }

    std::unique_ptr<IEncodedImage> CoverService::getFromSameNamedFile(const std::filesystem::path& filePath, ImageSize width) const
    {
        std::unique_ptr<IEncodedImage> res;

        std::filesystem::path coverPath{ filePath };
        for (const std::filesystem::path& extension : _fileExtensions)
        {
            coverPath.replace_extension(extension);

            if (!checkCoverFile(coverPath))
                continue;

            res = getFromCoverFile(coverPath, width);
            if (res)
                break;
        }

        return res;
    }

    bool CoverService::checkCoverFile(const std::filesystem::path& filePath) const
    {
        std::error_code ec;

        if (!isFileSupported(filePath, _fileExtensions))
            return false;

        if (!std::filesystem::exists(filePath, ec))
            return false;

        if (!std::filesystem::is_regular_file(filePath, ec))
            return false;

        if (std::filesystem::file_size(filePath, ec) > _maxFileSize && !ec)
        {
            LMS_LOG(COVER, INFO, "Image file '" << filePath.string() << " is too big (" << std::filesystem::file_size(filePath, ec) << "), limit is " << _maxFileSize);
            return false;
        }

        return true;
    }

    std::multimap<std::string, std::filesystem::path> CoverService::getCoverPaths(const std::filesystem::path& directoryPath) const
    {
        std::multimap<std::string, std::filesystem::path> res;
        std::error_code ec;

        std::filesystem::directory_iterator itPath(directoryPath, ec);
        std::filesystem::directory_iterator itEnd;
        while (!ec && itPath != itEnd)
        {
            const std::filesystem::path& path{ *itPath };

            if (checkCoverFile(path))
                res.emplace(std::filesystem::path{ path }.filename().replace_extension("").string(), path);

            itPath.increment(ec);
        }

        return res;
    }

    std::unique_ptr<IEncodedImage> CoverService::getFromTrack(const std::filesystem::path& p, ImageSize width) const
    {
        std::unique_ptr<IEncodedImage> image;

        try
        {
            image = getFromAvMediaFile(*Av::parseAudioFile(p), width);
        }
        catch (Av::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Cannot get covers from track " << p.string() << ": " << e.what());
        }

        return image;
    }

    std::shared_ptr<IEncodedImage> CoverService::getFromTrack(Database::TrackId trackId, ImageSize width)
    {
        return getFromTrack(_db.getTLSSession(), trackId, width, true /* allow release fallback*/);
    }

    std::shared_ptr<IEncodedImage> CoverService::getFromTrack(Database::Session& dbSession, Database::TrackId trackId, ImageSize width, bool allowReleaseFallback)
    {
        using namespace Database;

        const CacheEntryDesc cacheEntryDesc{ trackId, width };

        std::shared_ptr<IEncodedImage> cover{ loadFromCache(cacheEntryDesc) };
        if (cover)
            return cover;

        if (const std::optional<TrackInfo> trackInfo{ getTrackInfo(dbSession, trackId) })
        {
            if (trackInfo->hasCover)
                cover = getFromTrack(trackInfo->trackPath, width);

            if (!cover)
                cover = getFromSameNamedFile(trackInfo->trackPath, width);

            if (!cover && trackInfo->releaseId && allowReleaseFallback)
                cover = getFromRelease(*trackInfo->releaseId, width);

            if (!cover && trackInfo->isMultiDisc)
            {
                if (trackInfo->trackPath.parent_path().has_parent_path())
                    cover = getFromDirectory(trackInfo->trackPath.parent_path().parent_path(), width, _preferredFileNames, true);
            }
        }

        if (cover)
            saveToCache(cacheEntryDesc, cover);

        return cover;
    }

    std::shared_ptr<IEncodedImage> CoverService::getFromRelease(Database::ReleaseId releaseId, ImageSize width)
    {
        using namespace Database;
        const CacheEntryDesc cacheEntryDesc{ releaseId, width };

        std::shared_ptr<IEncodedImage> cover{ loadFromCache(cacheEntryDesc) };
        if (cover)
            return cover;

        struct ReleaseInfo
        {
            TrackId firstTrackId;
            std::filesystem::path releaseDirectory;
        };

        Session& session{ _db.getTLSSession() };

        auto getReleaseInfo{ [&]
        {
            std::optional<ReleaseInfo> res;

            auto transaction{ session.createReadTransaction() };

            // get a track in this release, consider the release is in a single directory
            const auto tracks{ Track::find(session, Track::FindParameters {}.setRelease(releaseId).setRange(Range{ 0, 1 }).setSortMethod(TrackSortMethod::Release)) };
            if (!tracks.results.empty())
            {
                const Track::pointer& track{ tracks.results.front() };
                res = ReleaseInfo{};
                res->firstTrackId = track->getId();
                res->releaseDirectory = track->getPath().parent_path();
            }

            return res;
        } };

        if (const std::optional<ReleaseInfo> releaseInfo{ getReleaseInfo() })
        {
            cover = getFromDirectory(releaseInfo->releaseDirectory, width, _preferredFileNames, true);
            if (!cover)
                cover = getFromTrack(session, releaseInfo->firstTrackId, width, false /* no release fallback */);
        }

        if (cover)
            saveToCache(cacheEntryDesc, cover);

        return cover;
    }

    std::shared_ptr<IEncodedImage> CoverService::getFromArtist(Database::ArtistId artistId, ImageSize width)
    {
        using namespace Database;
        const CacheEntryDesc cacheEntryDesc{ artistId, width };

        std::shared_ptr<IEncodedImage> artistImage{ loadFromCache(cacheEntryDesc) };
        if (artistImage)
            return artistImage;

        std::set<std::filesystem::path> parentPaths;
        {
            Session& session{ _db.getTLSSession() };

            Track::FindParameters params;
            params.setArtist(artistId, { TrackArtistLinkType::ReleaseArtist });

            auto transaction{ session.createReadTransaction() };

            Track::find(session, params, [&](const Track::pointer& track)
                {
                    parentPaths.insert(track->getPath().parent_path());
                });
        }

        if (parentPaths.size() == 1)
        {
            artistImage = getFromDirectory(parentPaths.begin()->parent_path(), width, _artistFileNames, false);
        }
        else if (parentPaths.size() > 1)
        {
            const std::filesystem::path longestCommonPath{ PathUtils::getLongestCommonPath(std::cbegin(parentPaths), std::cend(parentPaths)) };
            artistImage = getFromDirectory(longestCommonPath, width, _artistFileNames, false);
        }

        if (!artistImage)
        {
            for (const std::filesystem::path& parentPath : parentPaths)
            {
                artistImage = getFromDirectory(parentPath, width, _artistFileNames, false);
                if (artistImage)
                    break;
            }
        }

        if (artistImage)
            saveToCache(cacheEntryDesc, artistImage);

        return artistImage;
    }

    void CoverService::flushCache()
    {
        std::unique_lock lock{ _cacheMutex };

        LMS_LOG(COVER, DEBUG, "Cache stats: hits = " << _cacheHits << ", misses = " << _cacheMisses << ", nb entries = " << _cache.size() << ", size = " << _cacheSize);
        _cacheHits = 0;
        _cacheMisses = 0;
        _cacheSize = 0;
        _cache.clear();
    }

    void CoverService::setJpegQuality(unsigned quality)
    {
        _jpegQuality = Utils::clamp<unsigned>(quality, 1, 100);

        LMS_LOG(COVER, INFO, "JPEG export quality = " << _jpegQuality);
    }

    void CoverService::saveToCache(const CacheEntryDesc& entryDesc, std::shared_ptr<IEncodedImage> image)
    {
        std::unique_lock lock{ _cacheMutex };

        while (_cacheSize + image->getDataSize() > _maxCacheSize && !_cache.empty())
        {
            auto itRandom{ Random::pickRandom(_cache) };
            _cacheSize -= itRandom->second->getDataSize();
            _cache.erase(itRandom);
        }

        _cacheSize += image->getDataSize();
        _cache[entryDesc] = image;
    }

    std::shared_ptr<IEncodedImage> CoverService::loadFromCache(const CacheEntryDesc& entryDesc)
    {
        std::shared_lock lock{ _cacheMutex };

        auto it{ _cache.find(entryDesc) };
        if (it == std::cend(_cache))
        {
            ++_cacheMisses;
            return nullptr;
        }

        ++_cacheHits;
        return it->second;
    }

} // namespace Cover

