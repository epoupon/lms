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

#include "ArtworkService.hpp"

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
        bool isFileSupported(const std::filesystem::path& file, const std::vector<std::filesystem::path>& extensions)
        {
            return (std::find(std::cbegin(extensions), std::cend(extensions), file.extension()) != std::cend(extensions));
        }
    } // namespace

    std::unique_ptr<IArtworkService> createArtworkService(db::Db& db, const std::filesystem::path& defaultReleaseCoverSvgPath, const std::filesystem::path& defaultArtistImageSvgPath)
    {
        return std::make_unique<ArtworkService>(db, defaultReleaseCoverSvgPath, defaultArtistImageSvgPath);
    }

    using namespace image;

    ArtworkService::ArtworkService(db::Db& db,
        const std::filesystem::path& defaultReleaseCoverSvgPath,
        const std::filesystem::path& defaultArtistImageSvgPath)
        : _db{ db }
        , _cache{ core::Service<core::IConfig>::get()->getULong("cover-max-cache-size", 30) * 1000 * 1000 }
    {
        setJpegQuality(core::Service<core::IConfig>::get()->getULong("cover-jpeg-quality", 75));

        LMS_LOG(COVER, INFO, "Default release cover path = '" << defaultReleaseCoverSvgPath.string() << "'");
        LMS_LOG(COVER, INFO, "Max cache size = " << _cache.getMaxCacheSize());

        _defaultReleaseCover = image::readSvgFile(defaultReleaseCoverSvgPath); // may throw
        _defaultArtistImage = image::readSvgFile(defaultArtistImageSvgPath);   // may throw
    }

    std::unique_ptr<IEncodedImage> ArtworkService::getFromAvMediaFile(const av::IAudioFile& input, ImageSize width) const
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

    std::unique_ptr<IEncodedImage> ArtworkService::getFromImageFile(const std::filesystem::path& p, ImageSize width) const
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

    std::shared_ptr<IEncodedImage> ArtworkService::getDefaultReleaseCover()
    {
        return _defaultReleaseCover;
    }

    std::shared_ptr<IEncodedImage> ArtworkService::getDefaultArtistImage()
    {
        return _defaultArtistImage;
    }

    bool ArtworkService::checkImageFile(const std::filesystem::path& filePath) const
    {
        std::error_code ec;

        if (!isFileSupported(filePath, _fileExtensions))
            return false;

        if (!std::filesystem::exists(filePath, ec))
            return false;

        if (!std::filesystem::is_regular_file(filePath, ec))
            return false;

        return true;
    }

    std::unique_ptr<IEncodedImage> ArtworkService::getTrackImage(const std::filesystem::path& p, ImageSize width) const
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

    std::shared_ptr<IEncodedImage> ArtworkService::getTrackImage(db::TrackId trackId, ImageSize width)
    {
        const ImageCache::EntryDesc cacheEntryDesc{ trackId, width };

        std::shared_ptr<IEncodedImage> cover{ _cache.getImage(cacheEntryDesc) };
        if (cover)
            return cover;

        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            const db::Track::pointer track{ db::Track::find(session, trackId) };
            if (track && track->hasCover())
                cover = getTrackImage(track->getAbsoluteFilePath(), width);
        }

        if (cover)
            _cache.addImage(cacheEntryDesc, cover);

        return cover;
    }

    std::shared_ptr<IEncodedImage> ArtworkService::getReleaseCover(db::ReleaseId releaseId, ImageSize width)
    {
        using namespace db;
        const ImageCache::EntryDesc cacheEntryDesc{ releaseId, width };

        std::shared_ptr<IEncodedImage> image{ _cache.getImage(cacheEntryDesc) };
        if (image)
            return image;

        {
            Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            const db::Release::pointer release{ db::Release::find(session, releaseId) };
            if (release)
            {
                if (const db::Image::pointer dbImage{ release->getImage() })
                    image = getFromImageFile(dbImage->getAbsoluteFilePath(), width);
            }
        }

        if (image)
            _cache.addImage(cacheEntryDesc, image);

        return image;
    }

    std::shared_ptr<IEncodedImage> ArtworkService::getArtistImage(db::ArtistId artistId, ImageSize width)
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
                    artistImage = getFromImageFile(image->getAbsoluteFilePath(), width);
            }
        }

        if (artistImage)
            _cache.addImage(cacheEntryDesc, artistImage);

        return artistImage;
    }

    void ArtworkService::flushCache()
    {
        _cache.flush();
    }

    void ArtworkService::setJpegQuality(unsigned quality)
    {
        _jpegQuality = core::utils::clamp<unsigned>(quality, 1, 100);

        LMS_LOG(COVER, INFO, "JPEG export quality = " << _jpegQuality);
    }

} // namespace lms::cover
