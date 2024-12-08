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
#include "services/artwork/IArtworkService.hpp"

#include "LmsApplication.hpp"

#define LOG(severity, message) LMS_LOG(UI, severity, "Image resource: " << message)

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

    std::string ArtworkResource::getArtistImageUrl(db::ArtistId artistId, Size size) const
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

    std::string ArtworkResource::getReleaseCoverUrl(db::ReleaseId releaseId, Size size) const
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
                    db::Track::FindParameters params;
                    params.setRelease(releaseId);
                    params.setHasEmbeddedImage(true);
                    params.setRange(db::Range{ 0, 1 });

                    db::Track::find(LmsApp->getDbSession(), params, [&](const db::Track::pointer& track) {
                        url = getImageUrl(track->getId(), size, "release");
                    });
                }
            }
        }

        if (url.empty())
            url = getDefaultReleaseCoverUrl();

        return url;
    }

    std::string ArtworkResource::getTrackImageUrl(db::TrackId trackId, Size size) const
    {
        std::string url;

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), trackId) };
            if (track)
            {
                if (track->hasCover())
                {
                    url = getImageUrl(trackId, size, "release");
                }
                else if (const db::Release::pointer release{ track->getRelease() })
                {
                    if (const db::Image::pointer image{ release->getImage() })
                        url = getImageUrl(image->getId(), size, "release");
                }
            }
        }

        if (url.empty())
            url = getDefaultReleaseCoverUrl();

        return url;
    }

    std::string ArtworkResource::getImageUrl(db::ImageId imageId, Size size, std::string_view type) const
    {
        return url() + "&imageid=" + imageId.toString() + "&size=" + std::to_string(static_cast<std::size_t>(size)) + "&type=" + std::string{ type };
    }

    std::string ArtworkResource::getImageUrl(db::TrackId trackId, Size size, std::string_view type) const
    {
        return url() + "&trackid=" + trackId.toString() + "&size=" + std::to_string(static_cast<std::size_t>(size)) + "&type=" + std::string{ type };
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
        const std::string* trackIdStr = request.getParameter("trackid");
        const std::string* sizeStr = request.getParameter("size");
        const std::string* typeStr = request.getParameter("type");

        std::shared_ptr<image::IEncodedImage> image;

        if ((imageIdStr || trackIdStr))
        {
            const auto size{ sizeStr ? core::stringUtils::readAs<std::size_t>(*sizeStr) : std::nullopt };
            if (size && *size > maxSize)
            {
                LOG(DEBUG, "invalid size provided!");
                return;
            }

            if (imageIdStr)
            {
                const std::optional<db::ImageId> imageId{ core::stringUtils::readAs<db::ImageId::ValueType>(*imageIdStr) };
                if (!imageId)
                    return;

                image = core::Service<cover::IArtworkService>::get()->getImage(*imageId, size);
            }
            else if (trackIdStr)
            {
                const std::optional<db::TrackId> trackId{ core::stringUtils::readAs<db::TrackId::ValueType>(*trackIdStr) };
                if (!trackId)
                {
                    LOG(DEBUG, "track not found");
                    return;
                }

                image = core::Service<cover::IArtworkService>::get()->getTrackImage(*trackId, size);
            }
        }

        if (!image)
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
