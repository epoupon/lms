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

#include <algorithm>
#include <functional>

#include "av/IAudioFile.hpp"
#include "av/Types.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/String.hpp"
#include "core/Utils.hpp"
#include "database/Db.hpp"
#include "database/Image.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "image/Exception.hpp"
#include "image/IEncodedImage.hpp"
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

    ArtworkService::ArtworkService(db::Db& db,
        const std::filesystem::path& defaultReleaseCoverSvgPath,
        const std::filesystem::path& defaultArtistImageSvgPath)
        : _db{ db }
        , _cache{ core::Service<core::IConfig>::get()->getULong("cover-max-cache-size", 30) * 1000 * 1000 }
    {
        setJpegQuality(core::Service<core::IConfig>::get()->getULong("cover-jpeg-quality", 75));

        LMS_LOG(COVER, INFO, "Default release cover path = '" << defaultReleaseCoverSvgPath.string() << "'");
        LMS_LOG(COVER, INFO, "Max cache size = " << _cache.getMaxCacheSize());

        _defaultReleaseCover = image::readImage(defaultReleaseCoverSvgPath); // may throw
        _defaultArtistImage = image::readImage(defaultArtistImageSvgPath);   // may throw
    }

    std::unique_ptr<image::IEncodedImage> ArtworkService::getFromAvMediaFile(const av::IAudioFile& input, std::optional<image::ImageSize> width) const
    {
        struct CandidatePicture
        {
            av::Picture picture;
            bool isFront{};
            std::size_t index;

            // > means is better candidate
            bool operator>(const CandidatePicture& other) const
            {
                if (!isFront && other.isFront)
                    return false;
                if (isFront && !other.isFront)
                    return true;

                return index < other.index;
            }
        };

        auto metadataHasFrontKeyword{ [](const av::IAudioFile::MetadataMap& metadata) {
            return std::any_of(std::cbegin(metadata), std::cend(metadata), [](const auto& keyValue) { return core::stringUtils::stringCaseInsensitiveContains(keyValue.second, "front"); });
        } };

        std::vector<CandidatePicture> candidatePictures;
        std::size_t pictureIndex{};
        input.visitAttachedPictures([&](const av::Picture& picture, const av::IAudioFile::MetadataMap& metadata) {
            candidatePictures.emplace_back(picture, metadataHasFrontKeyword(metadata), pictureIndex++);
        });
        std::stable_sort(std::begin(candidatePictures), std::end(candidatePictures), std::greater<>());

        std::unique_ptr<image::IEncodedImage> image;
        for (const CandidatePicture& candidatePicture : candidatePictures)
        {
            try
            {
                if (!width)
                {
                    image = image::readImage(candidatePicture.picture.data, candidatePicture.picture.mimeType);
                }
                else
                {
                    auto rawImage{ image::decodeImage(candidatePicture.picture.data) };
                    rawImage->resize(*width);
                    image = image::encodeToJPEG(*rawImage, _jpegQuality);
                }
            }
            catch (const image::Exception& e)
            {
                LMS_LOG(COVER, ERROR, "Cannot read embedded cover: " << e.what());
            }
        }

        return image;
    }

    std::unique_ptr<image::IEncodedImage> ArtworkService::getFromImageFile(const std::filesystem::path& p, std::optional<image::ImageSize> width) const
    {
        std::unique_ptr<image::IEncodedImage> image;

        try
        {
            if (!width)
            {
                image = image::readImage(p);
            }
            else
            {
                auto rawImage{ image::decodeImage(p) };
                rawImage->resize(*width);
                image = image::encodeToJPEG(*rawImage, _jpegQuality);
            }
        }
        catch (const image::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Cannot read cover in file '" << p.string() << "': " << e.what());
        }

        return image;
    }

    std::shared_ptr<image::IEncodedImage> ArtworkService::getDefaultReleaseCover()
    {
        return _defaultReleaseCover;
    }

    std::shared_ptr<image::IEncodedImage> ArtworkService::getDefaultArtistImage()
    {
        return _defaultArtistImage;
    }

    bool ArtworkService::checkImageFile(const std::filesystem::path& filePath)
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

    std::unique_ptr<image::IEncodedImage> ArtworkService::getTrackImage(const std::filesystem::path& p, std::optional<image::ImageSize> width) const
    {
        std::unique_ptr<image::IEncodedImage> image;

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

    std::shared_ptr<image::IEncodedImage> ArtworkService::getImage(db::ImageId imageId, std::optional<image::ImageSize> width)
    {
        const ImageCache::EntryDesc cacheEntryDesc{ imageId, width };

        std::shared_ptr<image::IEncodedImage> cover{ _cache.getImage(cacheEntryDesc) };
        if (cover)
            return cover;

        std::filesystem::path imageFile;
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            const db::Image::pointer image{ db::Image::find(session, imageId) };
            if (image)
                imageFile = image->getAbsoluteFilePath();
        }

        cover = getFromImageFile(imageFile, width);
        if (cover)
            _cache.addImage(cacheEntryDesc, cover);

        return cover;
    }

    std::shared_ptr<image::IEncodedImage> ArtworkService::getTrackImage(db::TrackId trackId, std::optional<image::ImageSize> width)
    {
        const ImageCache::EntryDesc cacheEntryDesc{ trackId, width };

        std::shared_ptr<image::IEncodedImage> cover{ _cache.getImage(cacheEntryDesc) };
        if (cover)
            return cover;

        std::filesystem::path trackFile;
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            const db::Track::pointer track{ db::Track::find(session, trackId) };
            if (track && track->hasCover())
                trackFile = track->getAbsoluteFilePath();
        }

        cover = getTrackImage(trackFile, width);
        if (cover)
            _cache.addImage(cacheEntryDesc, cover);

        return cover;
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
