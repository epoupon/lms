/*
 * Copyright (C) 2014 Emeric Poupon
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

#include "ArtworkResource.hpp"

#include <Wt/Http/Response.h>
#include <Wt/WApplication.h>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "database/Artist.hpp"
#include "database/Image.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackEmbeddedImage.hpp"
#include "database/Types.hpp"
#include "services/artwork/IArtworkService.hpp"

#include "LmsApplication.hpp"

#define ARTWORK_RESOURCE_LOG(severity, message) LMS_LOG(UI, severity, "Image resource: " << message)

namespace lms::ui
{
    ArtworkResource::ArtworkResource()
    {
        LmsApp->getScannerEvents().scanComplete.connect(this, [this](const scanner::ScanStats& stats) {
            if (stats.nbChanges())
                setChanged();
        });
    }

    ArtworkResource::~ArtworkResource()
    {
        beingDeleted();
    }

    std::string ArtworkResource::getArtistImageUrl(db::ArtistId artistId, std::optional<Size> size) const
    {
        std::string url;

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const db::Artist::pointer artist{ db::Artist::find(LmsApp->getDbSession(), artistId) };
            if (artist)
            {
                if (const db::Image::pointer image{ artist->getImage() })
                    url = getImageUrl(image->getId(), size, "artist");
            }
        }

        if (url.empty())
            url = getDefaultArtistImageUrl();

        return url;
    }

    std::string ArtworkResource::getReleaseCoverUrl(db::ReleaseId releaseId, std::optional<Size> size) const
    {
        std::string url;

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const db::Release::pointer release{ db::Release::find(LmsApp->getDbSession(), releaseId) };
            if (release)
            {
                if (const db::Image::pointer image{ release->getImage() })
                {
                    url = getImageUrl(image->getId(), size, "release");
                }
                else
                {
                    db::TrackEmbeddedImage::FindParameters params;
                    params.setRelease(releaseId);
                    params.setIsPreferred(true);
                    params.setSortMethod(db::TrackEmbeddedImageSortMethod::FrontCoverAndSize);
                    params.setRange(db::Range{ 0, 1 });

                    db::TrackEmbeddedImage::find(LmsApp->getDbSession(), params, [&](const db::TrackEmbeddedImage::pointer& image) {
                        url = getImageUrl(image->getId(), size, "release");
                    });
                }
            }
        }

        if (url.empty())
            url = getDefaultReleaseCoverUrl();

        return url;
    }

    std::string ArtworkResource::getPreferredTrackImageUrl(db::TrackId trackId, std::optional<Size> size) const
    {
        std::string url;

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            db::TrackEmbeddedImage::FindParameters params;
            params.setTrack(trackId);
            params.setIsPreferred(true);
            params.setRange(db::Range{ .offset = 0, .size = 1 });

            db::TrackEmbeddedImage::find(LmsApp->getDbSession(), params, [&](const db::TrackEmbeddedImage::pointer& image) {
                url = getImageUrl(image->getId(), size, "release");
            });

            if (url.empty())
            {
                db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), trackId) };
                if (track)
                {
                    if (const db::Release::pointer release{ track->getRelease() })
                    {
                        if (const db::Image::pointer image{ release->getImage() })
                            url = getImageUrl(image->getId(), size, "release");
                    }
                }
            }
        }

        if (url.empty())
            url = getDefaultReleaseCoverUrl();

        return url;
    }

    std::string ArtworkResource::getImageUrl(db::ImageId imageId, std::optional<Size> size, std::string_view type) const
    {
        std::string res{ url() + "&imageid=" + imageId.toString() + "&type=" + std::string{ type } };
        if (size)
            res += "&size=" + std::to_string(static_cast<std::size_t>(*size));

        return res;
    }

    std::string ArtworkResource::getImageUrl(db::TrackEmbeddedImageId trackId, std::optional<Size> size, std::string_view type) const
    {
        std::string res{ url() + "&trimageid=" + trackId.toString() + "&type=" + std::string{ type } };
        if (size)
            res += "&size=" + std::to_string(static_cast<std::size_t>(*size));
        return res;
    }

    std::string ArtworkResource::getDefaultArtistImageUrl() const
    {
        return url() + "&type=artist";
    }

    std::string ArtworkResource::getDefaultReleaseCoverUrl() const
    {
        return url() + "&type=release";
    }

    void ArtworkResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        LMS_SCOPED_TRACE_OVERVIEW("UI", "HandleCoverRequest");

        // Retrieve parameters
        const std::string* imageIdStr = request.getParameter("imageid");
        const std::string* trackEmbeddedImageIdStr = request.getParameter("trimageid");
        const std::string* sizeStr = request.getParameter("size");
        const std::string* typeStr = request.getParameter("type");

        std::shared_ptr<image::IEncodedImage> image;

        if ((imageIdStr || trackEmbeddedImageIdStr))
        {
            const auto size{ sizeStr ? core::stringUtils::readAs<std::size_t>(*sizeStr) : std::nullopt };
            if (size && *size > maxSize)
            {
                ARTWORK_RESOURCE_LOG(DEBUG, "invalid size provided!");
                return;
            }

            if (imageIdStr)
            {
                if (const auto imageId{ core::stringUtils::readAs<db::ImageId::ValueType>(*imageIdStr) })
                    image = core::Service<cover::IArtworkService>::get()->getImage(*imageId, size);
            }
            else if (trackEmbeddedImageIdStr)
            {
                if (const auto imageId{ core::stringUtils::readAs<db::TrackEmbeddedImageId::ValueType>(*trackEmbeddedImageIdStr) })
                    image = core::Service<cover::IArtworkService>::get()->getTrackEmbeddedImage(*imageId, size);
            }
        }

        if (!image && typeStr)
        {
            if (*typeStr == "release")
                image = core::Service<cover::IArtworkService>::get()->getDefaultReleaseCover();
            else if (*typeStr == "artist")
                image = core::Service<cover::IArtworkService>::get()->getDefaultArtistImage();
        }

        if (image)
        {
            response.setMimeType(std::string{ image->getMimeType() });
            response.out().write(reinterpret_cast<const char*>(image->getData().data()), image->getData().size());
        }
        else
            response.setStatus(404);
    }
} // namespace lms::ui
