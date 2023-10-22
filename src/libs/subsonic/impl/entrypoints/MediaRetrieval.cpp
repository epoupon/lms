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

#include "MediaRetrieval.hpp"

#include "av/IAudioFile.hpp"
#include "av/TranscodeParameters.hpp"
#include "av/TranscodeResourceHandlerCreator.hpp"
#include "av/Types.hpp"
#include "services/cover/ICoverService.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/IResourceHandler.hpp"
#include "utils/Logger.hpp"
#include "utils/FileResourceHandlerCreator.hpp"
#include "utils/Utils.hpp"
#include "utils/String.hpp"
#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace {
        std::optional<Av::Format> subsonicStreamFormatToAvFormat(std::string_view format)
        {
            for (const auto& [str, avFormat] : std::initializer_list<std::pair<std::string_view, Av::Format>>{
                {"mp3", Av::Format::MP3},
                {"opus", Av::Format::OGG_OPUS},
                {"vorbis", Av::Format::OGG_VORBIS},
                })
            {
                if (StringUtils::stringCaseInsensitiveEqual("str", format))
                    return avFormat;
            }
            return std::nullopt;
        }

        Av::Format userTranscodeFormatToAvFormat(AudioFormat format)
        {
            switch (format)
            {
            case Database::AudioFormat::MP3:            return Av::Format::MP3;
            case Database::AudioFormat::OGG_OPUS:       return Av::Format::OGG_OPUS;
            case Database::AudioFormat::MATROSKA_OPUS:  return Av::Format::MATROSKA_OPUS;
            case Database::AudioFormat::OGG_VORBIS:     return Av::Format::OGG_VORBIS;
            case Database::AudioFormat::WEBM_VORBIS:    return Av::Format::WEBM_VORBIS;
            }
            return Av::Format::OGG_OPUS;
        }

        struct StreamParameters
        {
            Av::InputFileParameters inputFileParameters;
            std::optional<Av::TranscodeParameters> transcodeParameters;
            bool estimateContentLength{};
        };

        StreamParameters getStreamParameters(RequestContext& context)
        {
            // Mandatory params
            const TrackId id{ getMandatoryParameterAs<TrackId>(context.parameters, "id") };

            // Optional params
            std::size_t maxBitRate{ getParameterAs<std::size_t>(context.parameters, "maxBitRate").value_or(0) }; // "If set to zero, no limit is imposed"
            const std::string format{ getParameterAs<std::string>(context.parameters, "format").value_or("") };
            std::size_t timeOffset{ getParameterAs<std::size_t>(context.parameters, "timeOffset").value_or(0) };
            bool estimateContentLength{ getParameterAs<bool>(context.parameters, "estimateContentLength").value_or(false) };

            StreamParameters parameters;

            parameters.estimateContentLength = estimateContentLength;

            auto transaction{ context.dbSession.createSharedTransaction() };

            {
                const auto track{ Track::find(context.dbSession, id) };
                if (!track)
                    throw RequestedDataNotFoundError{};

                parameters.inputFileParameters.trackPath = track->getPath();
                parameters.inputFileParameters.duration = track->getDuration();
            }

            if (format == "raw") // raw => no transcode
                return parameters;

            const auto audioFile{ Av::parseAudioFile(parameters.inputFileParameters.trackPath) };

            // check if transcode is really needed or not
            // same format as requested, bitrate is lower than requested => no need to transcode
            if (const auto streamInfo{ audioFile->getBestStreamInfo() })
            {
                // assume reported codec is "mp3", "opus", "vorbis", etc.
                if (StringUtils::stringCaseInsensitiveEqual(streamInfo->codec, format) && (maxBitRate == 0 || (streamInfo->bitrate / 1000) <= maxBitRate))
                {
                    LMS_LOG(API_SUBSONIC, DEBUG) << "stream parameters are compatible with actual file: no transcode";
                    return parameters;
                }
            }

            const User::pointer user{ User::find(context.dbSession, context.userId) };
            if (!user)
                throw UserNotAuthorizedError{};

            Av::TranscodeParameters& transcodeParameters{ parameters.transcodeParameters.emplace() };

            transcodeParameters.stripMetadata = false; // We want clients to use metadata (offline use, replay gain, etc.)
            transcodeParameters.offset = std::chrono::seconds{ timeOffset };

            if (std::optional<Av::Format> requestedFormat{ subsonicStreamFormatToAvFormat(format) })
                transcodeParameters.format = *requestedFormat;
            else
                transcodeParameters.format = userTranscodeFormatToAvFormat(user->getSubsonicDefaultTranscodeFormat());

            transcodeParameters.bitrate = user->getSubsonicDefaultTranscodeBitrate();
            if (maxBitRate != 0)
                transcodeParameters.bitrate = Utils::clamp(transcodeParameters.bitrate, std::size_t{ 48000 }, maxBitRate * 1000);

            return parameters;
        }
    }

    void handleDownload(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        std::shared_ptr<IResourceHandler> resourceHandler;

        Wt::Http::ResponseContinuation* continuation{ request.continuation() };
        if (!continuation)
        {
            // Mandatory params
            Database::TrackId id{ getMandatoryParameterAs<Database::TrackId>(context.parameters, "id") };

            std::filesystem::path trackPath;
            {
                auto transaction{ context.dbSession.createSharedTransaction() };

                auto track{ Track::find(context.dbSession, id) };
                if (!track)
                    throw RequestedDataNotFoundError{};

                trackPath = track->getPath();
            }

            resourceHandler = createFileResourceHandler(trackPath);
        }
        else
        {
            resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(continuation->data());
        }

        continuation = resourceHandler->processRequest(request, response);
        if (continuation)
            continuation->setData(resourceHandler);
    }

    void handleStream(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        std::shared_ptr<IResourceHandler> resourceHandler;

        try
        {
            Wt::Http::ResponseContinuation* continuation = request.continuation();
            if (!continuation)
            {
                StreamParameters streamParameters{ getStreamParameters(context) };
                if (streamParameters.transcodeParameters)
                    resourceHandler = Av::createTranscodeResourceHandler(streamParameters.inputFileParameters, *streamParameters.transcodeParameters, streamParameters.estimateContentLength);
                else
                    resourceHandler = createFileResourceHandler(streamParameters.inputFileParameters.trackPath);
            }
            else
            {
                resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(continuation->data());
            }

            continuation = resourceHandler->processRequest(request, response);
            if (continuation)
                continuation->setData(resourceHandler);
        }
        catch (const Av::Exception& e)
        {
            LMS_LOG(API_SUBSONIC, ERROR) << "Caught Av exception: " << e.what();
        }
    }

    void handleGetCoverArt(RequestContext& context, const Wt::Http::Request& /*request*/, Wt::Http::Response& response)
    {
        // Mandatory params
        const auto trackId{ getParameterAs<TrackId>(context.parameters, "id") };
        const auto releaseId{ getParameterAs<ReleaseId>(context.parameters, "id") };

        if (!trackId && !releaseId)
            throw BadParameterGenericError{ "id" };

        std::size_t size{ getParameterAs<std::size_t>(context.parameters, "size").value_or(1024) };
        size = ::Utils::clamp(size, std::size_t{ 32 }, std::size_t{ 2048 });

        std::shared_ptr<Image::IEncodedImage> cover;
        if (trackId)
            cover = Service<Cover::ICoverService>::get()->getFromTrack(*trackId, size);
        else if (releaseId)
            cover = Service<Cover::ICoverService>::get()->getFromRelease(*releaseId, size);

        response.out().write(reinterpret_cast<const char*>(cover->getData()), cover->getDataSize());
        response.setMimeType(std::string{ cover->getMimeType() });
    }

} // namespace API::Subsonic
