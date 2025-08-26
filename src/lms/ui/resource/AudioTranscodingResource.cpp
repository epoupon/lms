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
#include "core/PseudoProtocols.hpp"

#include "audio/AudioProperties.hpp"
#include "audio/Exception.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "audio/IAudioFileInfoParser.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"
#include "services/transcoding/ITranscodeService.hpp"

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
        case db::TranscodingOutputFormat::OGG_OPUS:
        case db::TranscodingOutputFormat::OGG_VORBIS:
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
        std::optional<audio::AudioProperties> getAudioProperties(const std::filesystem::path& trackPath)
        {
            std::optional<audio::AudioProperties> res;

            try
            {
                const auto parser{ audio::createAudioFileInfoParser(audio::AudioFileInfoParserBackend::FFmpeg) };

                audio::AudioFileInfoParseOptions parseOptions;
                parseOptions.audioPropertiesReadStyle = audio::AudioFileInfoParseOptions::AudioPropertiesReadStyle::Average;
                parseOptions.readImages = false;
                parseOptions.readTags = false;

                const auto filePath = [&trackPath] () {
                    if (!core::track_on.matches(trackPath)) {
                        return trackPath;
                    }

                    const auto parsed = core::track_on.parseUri(trackPath);
                    if (const auto* err = std::get_if<std::string>(&parsed); err) {
                        LMS_LOG(UI, ERROR, "Cannot decode path " << trackPath);
                        return trackPath;
                    }

                    const auto& res = std::get<core::TrackOn::DecipheredURI>(parsed);
                    return res.path;
                } ();

                const auto audioFile{ parser->parse(filePath, parseOptions) };

                if (const audio::AudioProperties * properties{ audioFile->getAudioProperties() })
                    res = *properties;
            }
            catch (const audio::Exception& e)
            {
                LMS_LOG(UI, DEBUG, "Cannot parse audio properties in " << trackPath << ": " << e.what());
            }

            return res;
        }

        std::optional<audio::AudioProperties> getAudioProperties(const db::Track::pointer& track)
        {
            std::optional<audio::AudioProperties> res;

            if (track->getContainer() && track->getCodec())
            {
                res.emplace();
                res->container = *track->getContainer();
                res->codec = *track->getCodec();
                res->duration = track->getDuration();
                res->bitrate = track->getBitrate();
                res->channelCount = track->getChannelCount();
                res->sampleRate = track->getSampleRate();
                res->bitsPerSample = track->getBitsPerSample();
            }
            else
            {
                res = getAudioProperties(track->getAbsoluteFilePath());
            }

            return res;
        }

        std::optional<audio::TranscodeOutputFormat> audioFormatToTranscodingFormat(db::TranscodingOutputFormat format)
        {
            switch (format)
            {
            case db::TranscodingOutputFormat::MP3:
                return audio::TranscodeOutputFormat{ core::media::Container::MPEG, core::media::Codec::MP3 };
            case db::TranscodingOutputFormat::OGG_OPUS:
                return audio::TranscodeOutputFormat{ core::media::Container::Ogg, core::media::Codec::Opus };
            case db::TranscodingOutputFormat::OGG_VORBIS:
                return audio::TranscodeOutputFormat{ core::media::Container::Ogg, core::media::Codec::Vorbis };
            }

            TRANSCODE_LOG(ERROR, "Cannot convert from db audio format to transcoding format");

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

        std::optional<audio::TranscodeParameters> readTranscodingParameters(const Wt::Http::Request& request)
        {
            audio::TranscodeParameters parameters;

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

            const std::optional<audio::TranscodeOutputFormat> outputFormat{ audioFormatToTranscodingFormat(*format) };
            if (!outputFormat)
                return std::nullopt;

            // optional parameter
            std::size_t offset{ readParameterAs<std::size_t>(request, "offset").value_or(0) };

            {
                db::Session& session{ LmsApp->getDbSession() };

                const auto transaction{ session.createReadTransaction() };

                const db::Track::pointer track{ db::Track::find(session, *trackId) };
                if (!track)
                    return std::nullopt;

                parameters.inputParameters.filePath = track->getAbsoluteFilePath();
                const auto audioProperties{ getAudioProperties(track) };
                if (!audioProperties)
                    return std::nullopt;

                parameters.inputParameters.audioProperties = *audioProperties;
            }

            parameters.inputParameters.offset = std::chrono::seconds{ offset };
            parameters.outputParameters.stripMetadata = true;
            parameters.outputParameters.format = *outputFormat;
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
                resourceHandler = core::Service<transcoding::ITranscodeService>::get()->createTranscodeResourceHandler(*parameters);
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
