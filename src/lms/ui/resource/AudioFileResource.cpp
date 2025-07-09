/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "AudioFileResource.hpp"

#include <Wt/Http/Response.h>

#include "core/FileResourceHandlerCreator.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"

#include "LmsApplication.hpp"

namespace lms::ui
{
#define AUDIO_RESOURCE_LOG(severity, message) LMS_LOG(UI, severity, "Audio file resource: " << message)

    namespace
    {
        std::optional<std::filesystem::path> getTrackPathFromTrackId(db::TrackId trackId)
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), trackId) };
            if (!track)
            {
                AUDIO_RESOURCE_LOG(ERROR, "Missing track");
                return std::nullopt;
            }

            return track->getAbsoluteFilePath();
        }

        std::optional<std::filesystem::path> getTrackPathFromURLArgs(const Wt::Http::Request& request)
        {
            const std::string* trackIdParameter{ request.getParameter("trackid") };
            if (!trackIdParameter)
            {
                AUDIO_RESOURCE_LOG(ERROR, "Missing trackid URL parameter!");
                return std::nullopt;
            }

            const std::optional<db::TrackId> trackId{ core::stringUtils::readAs<db::TrackId::ValueType>(*trackIdParameter) };
            if (!trackId)
            {
                AUDIO_RESOURCE_LOG(ERROR, "Bad trackid URL parameter!");
                return std::nullopt;
            }

            return getTrackPathFromTrackId(*trackId);
        }

    } // namespace

    AudioFileResource::~AudioFileResource()
    {
        beingDeleted();
    }

    std::string AudioFileResource::getUrl(db::TrackId trackId) const
    {
        return url() + "&trackid=" + trackId.toString();
    }

    void AudioFileResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        LMS_SCOPED_TRACE_OVERVIEW("UI", "HandleAudioFileRequest");

        std::shared_ptr<core::IResourceHandler> fileResourceHandler;

        if (!request.continuation())
        {
            auto trackPath{ getTrackPathFromURLArgs(request) };
            if (!trackPath)
                return;

            fileResourceHandler = core::createFileResourceHandler(*trackPath);
        }
        else
        {
            fileResourceHandler = Wt::cpp17::any_cast<std::shared_ptr<core::IResourceHandler>>(request.continuation()->data());
        }

        auto* continuation{ fileResourceHandler->processRequest(request, response) };
        if (continuation)
            continuation->setData(fileResourceHandler);
    }
} // namespace lms::ui
