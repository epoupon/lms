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
#include "services/artwork/IArtworkService.hpp"

#include "LmsApplication.hpp"

#define ARTWORK_RESOURCE_LOG(severity, message) LMS_LOG(UI, severity, "Image resource: " << message)

namespace lms::ui
{
    namespace
    {
        std::string_view getDefaultArtworkPart(ArtworkResource::DefaultArtworkType fallbackType)
        {
            std::string_view res;
            switch (fallbackType)
            {
            case ArtworkResource::DefaultArtworkType::Release:
            case ArtworkResource::DefaultArtworkType::Track:
                res = "&fallback=defrelease";
                break;

            case ArtworkResource::DefaultArtworkType::Artist:
                res = "&fallback=defartist";
            }

            return res;
        }
    } // namespace

    ArtworkResource::ArtworkResource()
    {
        LmsApp->getScannerEvents().scanComplete.connect(this, [this](const scanner::ScanStats& stats) {
            if (stats.getChangesCount())
                setChanged();
        });
    }

    ArtworkResource::~ArtworkResource()
    {
        beingDeleted();
    }

    std::string ArtworkResource::getArtworkUrl(db::ArtworkId artworkId, DefaultArtworkType fallbackType, std::optional<Size> size) const
    {
        std::string res{ url() + "&artworkid=" + artworkId.toString() };
        if (size)
            res += "&size=" + std::to_string(static_cast<std::size_t>(*size));

        res += getDefaultArtworkPart(fallbackType);

        return res;
    }

    std::string ArtworkResource::getDefaultArtworkUrl(DefaultArtworkType type) const
    {
        std::string res{ url() };
        res += getDefaultArtworkPart(type);
        return res;
    }

    void ArtworkResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        LMS_SCOPED_TRACE_OVERVIEW("UI", "HandleCoverRequest");

        // Retrieve parameters
        const std::string* artworkIdStr = request.getParameter("artworkid");
        const std::string* sizeStr = request.getParameter("size");
        const std::string* fallbackStr = request.getParameter("fallback");

        std::shared_ptr<image::IEncodedImage> image;

        if (!artworkIdStr && !fallbackStr)
        {
            ARTWORK_RESOURCE_LOG(DEBUG, "no artwork ID or fallback provided");
            response.setStatus(400);
            return;
        }

        if (artworkIdStr)
        {
            const auto size{ sizeStr ? core::stringUtils::readAs<std::size_t>(*sizeStr) : std::nullopt };
            if (size && *size > maxSize)
            {
                ARTWORK_RESOURCE_LOG(DEBUG, "invalid size provided: " << *sizeStr);
                response.setStatus(400);
                return;
            }

            const auto artworkId{ core::stringUtils::readAs<db::ArtworkId::ValueType>(*artworkIdStr) };
            if (!artworkId)
            {
                ARTWORK_RESOURCE_LOG(DEBUG, "invalid artwork ID provided: '" << *artworkIdStr << "'");
                response.setStatus(400);
                return;
            }

            image = core::Service<artwork::IArtworkService>::get()->getImage(*artworkId, size);
            if (!image)
                ARTWORK_RESOURCE_LOG(DEBUG, "no image found for artwork ID: '" << *artworkIdStr << "'");
        }

        if (!image && fallbackStr)
        {
            if (*fallbackStr == "defartist")
                image = core::Service<artwork::IArtworkService>::get()->getDefaultArtistArtwork();
            else if (*fallbackStr == "defrelease")
                image = core::Service<artwork::IArtworkService>::get()->getDefaultReleaseArtwork();
            else
            {
                ARTWORK_RESOURCE_LOG(DEBUG, "invalid type provided: '" << *fallbackStr << "'");
                response.setStatus(400);
                return;
            }

            if (!image)
                ARTWORK_RESOURCE_LOG(DEBUG, "no default image found for type: '" << *fallbackStr << "'");
        }

        if (image)
        {
            response.setMimeType(std::string{ image->getMimeType() });
            response.out().write(reinterpret_cast<const char*>(image->getData().data()), image->getData().size());
        }
        else
        {
            response.setStatus(404);
        }
    }
} // namespace lms::ui
