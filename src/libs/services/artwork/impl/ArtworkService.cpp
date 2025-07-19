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

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Utils.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/ImageId.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackList.hpp"
#include "image/Exception.hpp"
#include "image/IEncodedImage.hpp"
#include "image/Image.hpp"
#include "metadata/IAudioFileParser.hpp"

namespace lms::artwork
{
    std::unique_ptr<IArtworkService> createArtworkService(db::IDb& db, const std::filesystem::path& defaultReleaseCoverSvgPath, const std::filesystem::path& defaultArtistImageSvgPath)
    {
        return std::make_unique<ArtworkService>(db, defaultReleaseCoverSvgPath, defaultArtistImageSvgPath);
    }

    ArtworkService::ArtworkService(db::IDb& db,
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

    std::shared_ptr<image::IEncodedImage> ArtworkService::getDefaultReleaseArtwork()
    {
        return _defaultReleaseCover;
    }

    std::shared_ptr<image::IEncodedImage> ArtworkService::getDefaultArtistArtwork()
    {
        return _defaultArtistImage;
    }

    std::unique_ptr<image::IEncodedImage> ArtworkService::getTrackImage(const std::filesystem::path& p, std::size_t index, std::optional<image::ImageSize> width) const
    {
        std::unique_ptr<image::IEncodedImage> image;

        try
        {
            std::size_t currentIndex{};

            _audioFileParser->parseImages(p, [&](const metadata::Image& parsedImage) {
                if (currentIndex++ != index)
                    return;

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
            });
        }
        catch (const metadata::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Cannot parse images from track " << p << ": " << e.what());
        }

        return image;
    }

    db::ArtworkId ArtworkService::findTrackListImage(db::TrackListId trackListId)
    {
        db::ArtworkId artworkId;

        // Iterate over all tracks and stop when we find an artwork
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        db::TrackList::pointer trackList{ db::TrackList::find(session, trackListId) };
        if (!trackList)
            return artworkId;

        const auto entries{ trackList->getEntries(db::Range{ 0, 10 }) };
        for (const auto& entry : entries.results)
        {
            const auto track{ entry->getTrack() };
            if (track->getPreferredMediaArtworkId().isValid())
            {
                artworkId = track->getPreferredMediaArtworkId();
                break; // stop iteration
            }

            if (track->getPreferredArtworkId().isValid())
            {
                artworkId = track->getPreferredArtworkId();
                break; // stop iteration
            }
        }

        return artworkId;
    }

    std::shared_ptr<image::IEncodedImage> ArtworkService::getImage(db::ArtworkId artworkId, std::optional<image::ImageSize> width)
    {
        const ImageCache::EntryDesc cacheEntryDesc{ artworkId, width };

        std::shared_ptr<image::IEncodedImage> image{ _cache.getImage(cacheEntryDesc) };
        if (image)
            return image;

        db::TrackEmbeddedImageId trackEmbeddedImageId;
        db::ImageId imageId;

        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::Artwork::pointer artwork{ db::Artwork::find(session, artworkId) };
            if (artwork)
            {
                trackEmbeddedImageId = artwork->getTrackEmbeddedImageId();
                imageId = artwork->getImageId();
            }
        }

        if (trackEmbeddedImageId.isValid())
            image = getTrackEmbeddedImage(trackEmbeddedImageId, width);
        else if (imageId.isValid())
            image = getImage(imageId, width);

        if (image)
            _cache.addImage(cacheEntryDesc, image);

        return image;
    }

    std::shared_ptr<image::IEncodedImage> ArtworkService::getImage(db::ImageId imageId, std::optional<image::ImageSize> width)
    {
        std::filesystem::path imageFile;
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            const db::Image::pointer image{ db::Image::find(session, imageId) };
            if (!image)
                return nullptr;

            imageFile = image->getAbsoluteFilePath();
        }

        return getFromImageFile(imageFile, width);
    }

    std::shared_ptr<image::IEncodedImage> ArtworkService::getTrackEmbeddedImage(db::TrackEmbeddedImageId trackEmbeddedImageId, std::optional<image::ImageSize> width)
    {
        std::shared_ptr<image::IEncodedImage> image;

        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            // TODO: could be put outside transaction
            db::TrackEmbeddedImageLink::find(session, trackEmbeddedImageId, [&](const db::TrackEmbeddedImageLink::pointer& link) {
                if (!image)
                    image = getTrackImage(link->getTrack()->getAbsoluteFilePath(), link->getIndex(), width);
            });
        }

        return image;
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
} // namespace lms::artwork
