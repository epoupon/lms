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

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Utils.hpp"
#include "database/Artist.hpp"
#include "database/Db.hpp"
#include "database/Image.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackEmbeddedImage.hpp"
#include "image/Exception.hpp"
#include "image/IEncodedImage.hpp"
#include "image/Image.hpp"
#include "metadata/IAudioFileParser.hpp"

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
        , _audioFileParser{ metadata::createAudioFileParser(metadata::AudioFileParserParameters{}) }
        , _cache{ core::Service<core::IConfig>::get()->getULong("cover-max-cache-size", 30) * 1000 * 1000 }
    {
        setJpegQuality(core::Service<core::IConfig>::get()->getULong("cover-jpeg-quality", 75));

        LMS_LOG(COVER, INFO, "Default release cover path = " << defaultReleaseCoverSvgPath);
        LMS_LOG(COVER, INFO, "Max cache size = " << _cache.getMaxCacheSize());

        _defaultReleaseCover = image::readImage(defaultReleaseCoverSvgPath); // may throw
        _defaultArtistImage = image::readImage(defaultArtistImageSvgPath);   // may throw
    }

    ArtworkService::~ArtworkService() = default;

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
            LMS_LOG(COVER, ERROR, "Cannot read cover in file " << p << ": " << e.what());
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
        struct CandidateImage
        {
            std::unique_ptr<image::IEncodedImage> image;
            bool isFront{};
            std::size_t index;

            // > means is better candidate
            bool operator>(const CandidateImage& other) const
            {
                if (!isFront && other.isFront)
                    return false;
                if (isFront && !other.isFront)
                    return true;

                return index < other.index;
            }
        };

        std::vector<CandidateImage> candidateImages;
        std::size_t pictureIndex{};

        try
        {
            _audioFileParser->parseImages(p, [&](const metadata::Image& parsedImage) {
                std::unique_ptr<image::IEncodedImage> image;
                try
                {
                    if (!width)
                    {
                        image = image::readImage(parsedImage.data, parsedImage.mimeType);
                    }
                    else
                    {
                        auto rawImage{ image::decodeImage(parsedImage.data) };
                        rawImage->resize(*width);
                        image = image::encodeToJPEG(*rawImage, _jpegQuality);
                    }
                }
                catch (const image::Exception& e)
                {
                    LMS_LOG(COVER, ERROR, "Cannot decode image from track " << p << ": " << e.what());
                }

                candidateImages.emplace_back(CandidateImage{ .image = std::move(image), .isFront = parsedImage.type == metadata::Image::Type::FrontCover, .index = pictureIndex++ });
            });
        }
        catch (const metadata::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Cannot parse images from track " << p << ": " << e.what());
        }

        std::stable_sort(std::begin(candidateImages), std::end(candidateImages), std::greater<>());
        if (!candidateImages.empty())
            return std::move(candidateImages.front().image);

        return {};
    }

    ArtworkService::ImageFindResult ArtworkService::findArtistImage(db::ArtistId artistId)
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        ImageFindResult res;

        if (const db::Artist::pointer artist{ db::Artist::find(session, artistId) })
        {
            if (const db::ImageId imageId{ artist->getImageId() }; imageId.isValid())
                res = imageId;

            // TODO fallback on first release?
        }

        return res;
    }

    ArtworkService::ImageFindResult ArtworkService::findPreferredTrackImage(db::TrackId trackId)
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };
        ImageFindResult res;

        db::TrackEmbeddedImage::FindParameters params;
        params.setTrack(trackId);
        params.setIsPreferred(true);
        params.setRange(db::Range{ .offset = 0, .size = 1 });

        db::TrackEmbeddedImage::find(session, params, [&](const db::TrackEmbeddedImage::pointer& image) {
            res = image->getId();
        });

        if (res.index() == 0)
        {
            if (db::Track::pointer track{ db::Track::find(session, trackId) })
            {
                if (const db::Release::pointer release{ track->getRelease() })
                {
                    if (const db::ImageId imageId{ release->getImageId() }; imageId.isValid())
                        res = imageId;
                }
            }
        }

        return res;
    }

    ArtworkService::ImageFindResult ArtworkService::findReleaseImage(db::ReleaseId releaseId)
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        ImageFindResult res;

        const db::Release::pointer release{ db::Release::find(session, releaseId) };
        if (release)
        {
            if (const db::ImageId imageId{ release->getImageId() }; imageId.isValid())
            {
                res = imageId;
            }
            else
            {
                db::TrackEmbeddedImage::FindParameters params;
                params.setRelease(releaseId);
                params.setIsPreferred(true);
                params.setSortMethod(db::TrackEmbeddedImageSortMethod::FrontCoverAndSize);
                params.setRange(db::Range{ .offset = 0, .size = 1 });

                db::TrackEmbeddedImage::find(session, params, [&](const db::TrackEmbeddedImage::pointer& image) {
                    res = image->getId();
                });
            }
        }

        return res;
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

    std::shared_ptr<image::IEncodedImage> ArtworkService::getTrackEmbeddedImage(db::TrackEmbeddedImageId trackEmbeddedImageId, std::optional<image::ImageSize> width)
    {
        const ImageCache::EntryDesc cacheEntryDesc{ trackEmbeddedImageId, width };

        std::shared_ptr<image::IEncodedImage> cover{ _cache.getImage(cacheEntryDesc) };
        if (cover)
            return cover;

        std::filesystem::path trackFile;
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setEmbeddedImage(trackEmbeddedImageId);
            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                if (!cover)
                    cover = getTrackImage(track->getAbsoluteFilePath(), width);
            });
        }

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
