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
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "core/Random.hpp"
#include "core/String.hpp"
#include "core/Utils.hpp"
#include "database/Artist.hpp"
#include "database/Db.hpp"
#include "database/Image.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"

namespace lms::cover
{
    namespace
    {
        struct TrackInfo
        {
            bool hasCover{};
            bool isMultiDisc{};
            std::filesystem::path trackPath;
            std::optional<db::ReleaseId> releaseId;
        };

        std::optional<TrackInfo> getTrackInfo(db::Session& dbSession, db::TrackId trackId)
        {
            std::optional<TrackInfo> res;

            auto transaction{ dbSession.createReadTransaction() };

            const db::Track::pointer track{ db::Track::find(dbSession, trackId) };
            if (!track)
                return res;

            res = TrackInfo{};

            res->hasCover = track->hasCover();
            res->trackPath = track->getAbsoluteFilePath();

            if (const db::Release::pointer & release{ track->getRelease() })
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

            core::Service<core::IConfig>::get()->visitStrings("cover-preferred-file-names",
                [&res](std::string_view fileName) {
                    res.emplace_back(fileName);
                },
                { "cover", "front" });

            return res;
        }

        bool isFileSupported(const std::filesystem::path& file, const std::vector<std::filesystem::path>& extensions)
        {
            return (std::find(std::cbegin(extensions), std::cend(extensions), file.extension()) != std::cend(extensions));
        }
    } // namespace

    std::unique_ptr<ICoverService> createCoverService(db::Db& db, const std::filesystem::path& defaultSvgCoverPath)
    {
        return std::make_unique<CoverService>(db, defaultSvgCoverPath);
    }

    using namespace image;

    CoverService::CoverService(db::Db& db,
        const std::filesystem::path& defaultSvgCoverPath)
        : _db{ db }
        , _cache{ core::Service<core::IConfig>::get()->getULong("cover-max-cache-size", 30) * 1000 * 1000 }
        , _maxFileSize{ core::Service<core::IConfig>::get()->getULong("cover-max-file-size", 10) * 1000 * 1000 }
        , _preferredFileNames{ constructPreferredFileNames() }
    {
        setJpegQuality(core::Service<core::IConfig>::get()->getULong("cover-jpeg-quality", 75));

        LMS_LOG(COVER, INFO, "Default cover path = '" << defaultSvgCoverPath.string() << "'");
        LMS_LOG(COVER, INFO, "Max cache size = " << _cache.getMaxCacheSize());
        LMS_LOG(COVER, INFO, "Max file size = " << _maxFileSize);
        LMS_LOG(COVER, INFO, "Preferred file names: " << core::stringUtils::joinStrings(_preferredFileNames, ","));

        _defaultCover = image::readSvgFile(defaultSvgCoverPath); // may throw
    }

    std::unique_ptr<IEncodedImage> CoverService::getFromAvMediaFile(const av::IAudioFile& input, ImageSize width) const
    {
        std::unique_ptr<IEncodedImage> image;

        input.visitAttachedPictures([&](const av::Picture& picture) {
            if (image)
                return;

            try
            {
                std::unique_ptr<IRawImage> rawImage{ decodeImage(picture.data, picture.dataSize) };
                rawImage->resize(width);
                image = rawImage->encodeToJPEG(_jpegQuality);
            }
            catch (const image::Exception& e)
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
        catch (const image::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Cannot read cover in file '" << p.string() << "': " << e.what());
        }

        return image;
    }

    std::shared_ptr<IEncodedImage> CoverService::getDefaultSvgCover()
    {
        return _defaultCover;
    }

    std::unique_ptr<IEncodedImage> CoverService::getFromDirectory(const std::filesystem::path& directory, ImageSize width, const std::vector<std::string>& preferredFileNames, bool allowPickRandom) const
    {
        const std::multimap<std::string, std::filesystem::path> coverPaths{ getCoverPaths(directory) };

        auto tryLoadImageFromFilename = [&](std::string_view fileName) {
            std::unique_ptr<IEncodedImage> image;

            auto range{ coverPaths.equal_range(std::string{ fileName }) };
            for (auto it{ range.first }; it != range.second; ++it)
            {
                image = getFromCoverFile(it->second, width);
                if (image)
                    break;
            }
            return image;
        };

        std::unique_ptr<IEncodedImage> image;

        for (const std::string_view filename : preferredFileNames)
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
        const std::filesystem::directory_iterator itEnd;
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
            image = getFromAvMediaFile(*av::parseAudioFile(p), width);
        }
        catch (av::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Cannot get covers from track " << p.string() << ": " << e.what());
        }

        return image;
    }

    std::shared_ptr<IEncodedImage> CoverService::getFromTrack(db::TrackId trackId, ImageSize width)
    {
        return getFromTrack(_db.getTLSSession(), trackId, width, true /* allow release fallback*/);
    }

    std::shared_ptr<IEncodedImage> CoverService::getFromTrack(db::Session& dbSession, db::TrackId trackId, ImageSize width, bool allowReleaseFallback)
    {
        using namespace db;

        const ImageCache::EntryDesc cacheEntryDesc{ trackId, width };

        std::shared_ptr<IEncodedImage> cover{ _cache.getImage(cacheEntryDesc) };
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
            _cache.addImage(cacheEntryDesc, cover);

        return cover;
    }

    std::shared_ptr<IEncodedImage> CoverService::getFromRelease(db::ReleaseId releaseId, ImageSize width)
    {
        using namespace db;
        const ImageCache::EntryDesc cacheEntryDesc{ releaseId, width };

        std::shared_ptr<IEncodedImage> cover{ _cache.getImage(cacheEntryDesc) };
        if (cover)
            return cover;

        struct ReleaseInfo
        {
            TrackId firstTrackId;
            std::filesystem::path releaseDirectory;
        };

        Session& session{ _db.getTLSSession() };

        auto getReleaseInfo{ [&] {
            std::optional<ReleaseInfo> res;

            auto transaction{ session.createReadTransaction() };

            // get a track in this release, consider the release is in a single directory
            const auto tracks{ Track::find(session, Track::FindParameters{}.setRelease(releaseId).setRange(Range{ 0, 1 }).setSortMethod(TrackSortMethod::Release)) };
            if (!tracks.results.empty())
            {
                const Track::pointer& track{ tracks.results.front() };
                res = ReleaseInfo{};
                res->firstTrackId = track->getId();
                res->releaseDirectory = track->getAbsoluteFilePath().parent_path();
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
            _cache.addImage(cacheEntryDesc, cover);

        return cover;
    }

    std::shared_ptr<IEncodedImage> CoverService::getFromArtist(db::ArtistId artistId, ImageSize width)
    {
        using namespace db;
        const ImageCache::EntryDesc cacheEntryDesc{ artistId, width };

        std::shared_ptr<IEncodedImage> artistImage{ _cache.getImage(cacheEntryDesc) };
        if (artistImage)
            return artistImage;

        {
            Session& session{ _db.getTLSSession() };

            auto transaction{ session.createReadTransaction() };

            if (const Artist::pointer artist{ Artist::find(session, artistId) })
            {
                if (const db::Image::pointer image{ artist->getImage() })
                    artistImage = getFromCoverFile(image->getAbsoluteFilePath(), width);
            }
        }

        if (artistImage)
            _cache.addImage(cacheEntryDesc, artistImage);

        return artistImage;
    }

    void CoverService::flushCache()
    {
        _cache.flush();
    }

    void CoverService::setJpegQuality(unsigned quality)
    {
        _jpegQuality = core::utils::clamp<unsigned>(quality, 1, 100);

        LMS_LOG(COVER, INFO, "JPEG export quality = " << _jpegQuality);
    }

} // namespace lms::cover
