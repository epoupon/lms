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

#include "CoverResource.hpp"

#include <Wt/WApplication.h>
#include <Wt/Http/Response.h>

#include "services/cover/ICoverService.hpp"
#include "database/Track.hpp"
#include "core/Exception.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"

#include "LmsApplication.hpp"

#define LOG(severity, message)	LMS_LOG(UI, severity, "Image resource: " << message)

namespace lms::ui
{
    CoverResource::CoverResource()
    {
        LmsApp->getScannerEvents().scanComplete.connect(this, [this](const scanner::ScanStats& stats)
            {
                if (stats.nbChanges())
                    setChanged();
            });
    }

    CoverResource::~CoverResource()
    {
        beingDeleted();
    }

    std::string CoverResource::getReleaseUrl(db::ReleaseId releaseId, Size size) const
    {
        return url() + "&releaseid=" + releaseId.toString() + "&size=" + std::to_string(static_cast<std::size_t>(size));
    }

    std::string CoverResource::getTrackUrl(db::TrackId trackId, Size size) const
    {
        return url() + "&trackid=" + trackId.toString() + "&size=" + std::to_string(static_cast<std::size_t>(size));
    }

    void CoverResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        LMS_SCOPED_TRACE_OVERVIEW("UI", "HandleCoverRequest");

        // Retrieve parameters
        const std::string* trackIdStr = request.getParameter("trackid");
        const std::string* releaseIdStr = request.getParameter("releaseid");
        const std::string* sizeStr = request.getParameter("size");

        // Mandatory parameter size
        if (!sizeStr)
        {
            LOG(DEBUG, "no size provided!");
            return;
        }

        const auto size{ core::stringUtils::readAs<std::size_t>(*sizeStr) };
        if (!size || *size > maxSize)
        {
            LOG(DEBUG, "invalid size provided!");
            return;
        }

        std::shared_ptr<image::IEncodedImage> cover;

        if (trackIdStr)
        {
            LOG(DEBUG, "Requested cover for track " << *trackIdStr << ", size = " << *size);

            const std::optional<db::TrackId> trackId{ core::stringUtils::readAs<db::TrackId::ValueType>(*trackIdStr) };
            if (!trackId)
            {
                LOG(DEBUG, "track not found");
                return;
            }

            cover = core::Service<cover::ICoverService>::get()->getFromTrack(*trackId, *size);
            if (!cover)
                cover = core::Service<cover::ICoverService>::get()->getDefaultSvgCover();
        }
        else if (releaseIdStr)
        {
            LOG(DEBUG, "Requested cover for release " << *releaseIdStr << ", size = " << *size);

            const std::optional<db::ReleaseId> releaseId{ core::stringUtils::readAs<db::ReleaseId::ValueType>(*releaseIdStr) };
            if (!releaseId)
                return;

            cover = core::Service<cover::ICoverService>::get()->getFromRelease(*releaseId, *size);
            if (!cover)
                cover = core::Service<cover::ICoverService>::get()->getDefaultSvgCover();
        }
        else
        {
            LOG(DEBUG, "No track or release provided");
            return;
        }

        response.setMimeType(std::string{ cover->getMimeType() });

        response.out().write(reinterpret_cast<const char*>(cover->getData()), cover->getDataSize());
    }
} // namespace lms::ui
