/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "AudioTranscodingResource.hpp"

#include <optional>

#include <Wt/Http/Response.h>

#include "core/ILogger.hpp"
#include "core/IResourceHandler.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"
#include "services/transcoding/ITranscodingService.hpp"

#include "LmsApplication.hpp"

#define TRANSCODE_LOG(severity, message) LMS_LOG(UI, severity, "Audio transcode resource: " << message)

namespace lms::core::stringUtils
{
    template<>
    std::optional<db::TranscodingOutputFormat> readAs(std::string_view str)
    {
        auto encodedFormat{ readAs<int>(str) };
        if (!encodedFormat)
            return std::nullopt;

        db::TranscodingOutputFormat format{ static_cast<db::TranscodingOutputFormat>(*encodedFormat) };

        // check
        switch (static_cast<db::TranscodingOutputFormat>(*encodedFormat))
        {
        case db::TranscodingOutputFormat::MP3:
            [[fallthrough]];
        case db::TranscodingOutputFormat::OGG_OPUS:
            [[fallthrough]];
        case db::TranscodingOutputFormat::MATROSKA_OPUS:
            [[fallthrough]];
        case db::TranscodingOutputFormat::OGG_VORBIS:
            [[fallthrough]];
        case db::TranscodingOutputFormat::WEBM_VORBIS:
            return format;
        }

        TRANSCODE_LOG(ERROR, "Cannot determine audio format from value '" << str << "'");

        return std::nullopt;
    }
} // namespace lms::core::stringUtils

namespace lms::ui
{
    namespace
    {
        std::optional<transcoding::OutputFormat> AudioFormatToAvFormat(db::TranscodingOutputFormat format)
        {
            switch (format)
            {
            case db::TranscodingOutputFormat::MP3:
                return transcoding::OutputFormat::MP3;
            case db::TranscodingOutputFormat::OGG_OPUS:
                return transcoding::OutputFormat::OGG_OPUS;
            case db::TranscodingOutputFormat::MATROSKA_OPUS:
                return transcoding::OutputFormat::MATROSKA_OPUS;
            case db::TranscodingOutputFormat::OGG_VORBIS:
                return transcoding::OutputFormat::OGG_VORBIS;
            case db::TranscodingOutputFormat::WEBM_VORBIS:
                return transcoding::OutputFormat::WEBM_VORBIS;
            }

            TRANSCODE_LOG(ERROR, "Cannot convert from audio format to AV format");

            return std::nullopt;
        }

        template<typename T>
        std::optional<T> readParameterAs(const Wt::Http::Request& request, const std::string& parameterName)
        {
            auto paramStr{ request.getParameter(parameterName) };
            if (!paramStr)
            {
                TRANSCODE_LOG(DEBUG, "Missing parameter '" << parameterName << "'");
                return std::nullopt;
            }

            auto res{ core::stringUtils::readAs<T>(*paramStr) };
            if (!res)
                TRANSCODE_LOG(ERROR, "Cannot parse parameter '" << parameterName << "' from value '" << *paramStr << "'");

            return res;
        }

        struct TranscodingParameters
        {
            transcoding::InputParameters inputParameters;
            transcoding::OutputParameters outputParameters;
        };

        std::optional<TranscodingParameters> readTranscodingParameters(const Wt::Http::Request& request)
        {
            TranscodingParameters parameters;

            // mandatory parameters
            const std::optional<db::TrackId> trackId{ readParameterAs<db::TrackId::ValueType>(request, "trackid") };
            const auto format{ readParameterAs<db::TranscodingOutputFormat>(request, "format") };
            const auto bitrate{ readParameterAs<db::Bitrate>(request, "bitrate") };

            if (!trackId || !format || !bitrate)
                return std::nullopt;

            if (!db::isAudioBitrateAllowed(*bitrate))
            {
                TRANSCODE_LOG(ERROR, "Bitrate '" << *bitrate << "' is not allowed");
                return std::nullopt;
            }

            const std::optional<transcoding::OutputFormat> avFormat{ AudioFormatToAvFormat(*format) };
            if (!avFormat)
                return std::nullopt;

            // optional parameter
            std::size_t offset{ readParameterAs<std::size_t>(request, "offset").value_or(0) };

            parameters.inputParameters.trackId = *trackId;
            parameters.inputParameters.offset = std::chrono::seconds{ offset };
            parameters.outputParameters.stripMetadata = true;
            parameters.outputParameters.format = *avFormat;
            parameters.outputParameters.bitrate = *bitrate;

            return parameters;
        }
    } // namespace

    AudioTranscodingResource::~AudioTranscodingResource()
    {
        beingDeleted();
    }

    std::string AudioTranscodingResource::getUrl(db::TrackId trackId) const
    {
        return url() + "&trackid=" + trackId.toString();
    }

    void AudioTranscodingResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        std::shared_ptr<core::IResourceHandler> resourceHandler;

        Wt::Http::ResponseContinuation* continuation{ request.continuation() };
        if (!continuation)
        {
            if (const auto& parameters{ readTranscodingParameters(request) })
                resourceHandler = core::Service<transcoding::ITranscodingService>::get()->createResourceHandler(parameters->inputParameters, parameters->outputParameters, false /* estimate content length */);
        }
        else
        {
            resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<core::IResourceHandler>>(continuation->data());
        }

        if (resourceHandler)
        {
            continuation = resourceHandler->processRequest(request, response);
            if (continuation)
                continuation->setData(resourceHandler);
        }
    }

} // namespace lms::ui