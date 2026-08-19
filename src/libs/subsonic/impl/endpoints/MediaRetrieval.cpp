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

#include <algorithm>
#include <chrono>

#include "core/FileResourceHandlerCreator.hpp"
#include "core/ILogger.hpp"
#include "core/IResourceHandler.hpp"
#include "core/String.hpp"
#include "core/media/Codec.hpp"
#include "core/media/MimeType.hpp"

#include "database/Session.hpp"
#include "database/objects/PodcastEpisodeId.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/User.hpp"

#include "services/artwork/IArtworkService.hpp"
#include "services/transcoding/ITranscodeService.hpp"

#include "ParameterParsing.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"
#include "responses/Lyrics.hpp"
#include "transcoding/AudioFileInfo.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        struct OutputFormat
        {
            std::string_view name;
            core::media::Container container;
            core::media::Codec codec;
        };
        constexpr std::array outputFormats{
            OutputFormat{ .name = "mp3", .container = core::media::Container::MPEG, .codec = core::media::Codec::MP3 },
            OutputFormat{ .name = "opus", .container = core::media::Container::Ogg, .codec = core::media::Codec::Opus },
            OutputFormat{ .name = "vorbis", .container = core::media::Container::Ogg, .codec = core::media::Codec::Vorbis },
            OutputFormat{ .name = "flac", .container = core::media::Container::FLAC, .codec = core::media::Codec::FLAC },
        };

        std::optional<OutputFormat> getOutputFormatByName(std::string_view format)
        {
            std::optional<OutputFormat> res;

            const auto itFormat{ std::find_if(outputFormats.begin(), outputFormats.end(), [&format, &res](const OutputFormat& outputFormat) {
                if (core::stringUtils::stringCaseInsensitiveEqual(format, outputFormat.name))
                {
                    res = outputFormat;
                    return true;
                }
                return false;
            }) };
            if (itFormat != outputFormats.end())
                res = *itFormat;

            return res;
        }

        OutputFormat userTranscodeFormatToOutputFormat(db::TranscodingOutputFormat format)
        {
            std::optional<OutputFormat> res;

            switch (format)
            {
            case db::TranscodingOutputFormat::MP3:
                res = getOutputFormatByName("mp3");
                break;
            case db::TranscodingOutputFormat::OGG_OPUS:
                res = getOutputFormatByName("opus");
                break;
            case db::TranscodingOutputFormat::OGG_VORBIS:
                res = getOutputFormatByName("vorbis");
                break;
            }

            if (!res)
                throw InternalErrorGenericError{ "User's default transcoding format is invalid" };

            LMS_LOG(API_SUBSONIC, DEBUG, "Using user's default transcoding format: " << res->name);
            return *res;
        }

        struct StreamParameters
        {
            std::filesystem::path filePath;
            audio::AudioProperties audioProperties;
            std::optional<audio::TranscodeParameters> transcodeParameters;
            bool estimateContentLength{};
        };

        StreamParameters getStreamParameters(RequestContext& context)
        {
            // Mandatory params
            const auto trackId{ getParameterAs<db::TrackId>(context.getParameters(), "id") };
            const auto podcastEpisodeId{ getParameterAs<db::PodcastEpisodeId>(context.getParameters(), "id") };
            if (!trackId && !podcastEpisodeId)
                throw RequiredParameterMissingError{ "id" };

            const AudioFileId audioId{ trackId ? AudioFileId{ *trackId } : AudioFileId{ *podcastEpisodeId } };

            // Optional params
            const std::size_t maxBitRate{ getParameterAs<std::size_t>(context.getParameters(), "maxBitRate").value_or(0) * 1000 }; // "If set to zero, no limit is imposed", given in kpbs
            const std::string format{ getParameterAs<std::string>(context.getParameters(), "format").value_or("") };
            const std::size_t timeOffset{ getParameterAs<std::size_t>(context.getParameters(), "timeOffset").value_or(0) };
            const bool estimateContentLength{ getParameterAs<bool>(context.getParameters(), "estimateContentLength").value_or(false) };

            const AudioFileInfo audioFileInfo{ getAudioFileInfo(context.getDbSession(), audioId) };

            StreamParameters parameters;
            parameters.audioProperties = audioFileInfo.audioProperties;
            parameters.filePath = audioFileInfo.path;
            parameters.estimateContentLength = estimateContentLength;

            if (format == "raw")   // raw => no transcoding
                return parameters; // TODO: what if offset is not 0?

            std::optional<OutputFormat> requestedFormat;

            if (!format.empty())
            {
                requestedFormat = getOutputFormatByName(format);
                if (!requestedFormat)
                    LMS_LOG(API_SUBSONIC, ERROR, "Format '" << format << "' is not recognized: ignoring");
            }

            if (!requestedFormat)
            {
                if (context.getUser()->getSubsonicEnableTranscodingByDefault())
                    requestedFormat = userTranscodeFormatToOutputFormat(context.getUser()->getSubsonicDefaultTranscodingOutputFormat());
            }

            // Extra checks when requesting a lossless format
            if (requestedFormat && core::media::getCodecDesc(requestedFormat->codec).isLossless)
            {
                if (!core::media::getCodecDesc(audioFileInfo.audioProperties.codec).isLossless && maxBitRate > 0)
                    throw BadParameterGenericError{ "maxBitRate", "Cannot limit bitrate when requesting a lossless format from a lossy source" };

                if (core::media::getCodecDesc(audioFileInfo.audioProperties.codec).isLossless && maxBitRate > 0 && audioFileInfo.audioProperties.bitrate > maxBitRate)
                    throw BadParameterGenericError{ "maxBitRate", "Cannot honor maxBitRate, source is lossless with higher bitrate (" + std::to_string(audioFileInfo.audioProperties.bitrate) + " bps)" };

                // we accept to transcode from a lossy format to a lossless one
            }

            if (!requestedFormat && (maxBitRate == 0 || audioFileInfo.audioProperties.bitrate <= maxBitRate))
            {
                LMS_LOG(API_SUBSONIC, DEBUG, "File's bitrate (" << audioFileInfo.audioProperties.bitrate << " bps) is compatible with parameters => no transcoding");
                return parameters; // no transcoding needed
            }

            // Check if the input file is compatible with the requested format
            std::optional<std::size_t> bitrate;
            if (requestedFormat && requestedFormat->container == audioFileInfo.audioProperties.container && requestedFormat->codec == audioFileInfo.audioProperties.codec)
            {
                //  same codec => check if compatible with max bitrate
                if (maxBitRate == 0 || audioFileInfo.audioProperties.bitrate <= maxBitRate)
                {
                    LMS_LOG(API_SUBSONIC, DEBUG, "File's bitrate (" << audioFileInfo.audioProperties.bitrate << " bps) and format are compatible with parameters => no transcoding");
                    return parameters; // no transcoding needed
                }

                // otherwise => apply requested bitrate
                assert(maxBitRate > 0);
                bitrate = maxBitRate;
            }

            // Need to transcode here
            audio::TranscodeParameters& transcodeParameters{ parameters.transcodeParameters.emplace() };

            if (!requestedFormat) // no format provided => use user's default
                requestedFormat = userTranscodeFormatToOutputFormat(context.getUser()->getSubsonicDefaultTranscodingOutputFormat());

            assert(requestedFormat);
            if (!core::media::getCodecDesc(requestedFormat->codec).isLossless)
            {
                // Try to keep a bitrate as close as possible to the source one
                // If the source is lossless, we need to pick a default bitrate
                if (core::media::getCodecDesc(audioFileInfo.audioProperties.codec).isLossless)
                {
                    bitrate = context.getUser()->getSubsonicDefaultTranscodingOutputBitrate();
                    LMS_LOG(API_SUBSONIC, DEBUG, "Source is lossless, selecting user's default transcoding bitrate (" << *bitrate << " bps) for transcoding");
                }
                else
                {
                    bitrate = audioFileInfo.audioProperties.bitrate;
                    LMS_LOG(API_SUBSONIC, DEBUG, "Source is lossy, selecting source bitrate (" << *bitrate << " bps) for transcoding");
                }

                if (maxBitRate > 0) // Honor provided maxBitRate, if any
                    bitrate = std::min<std::size_t>(*bitrate, maxBitRate);
            }

            transcodeParameters.inputParameters.filePath = audioFileInfo.path;
            transcodeParameters.inputParameters.audioProperties = audioFileInfo.audioProperties;
            transcodeParameters.inputParameters.offset = std::chrono::seconds{ timeOffset };

            transcodeParameters.outputParameters.bitrate = bitrate;
            transcodeParameters.outputParameters.format.emplace();
            transcodeParameters.outputParameters.format->container = requestedFormat->container;
            transcodeParameters.outputParameters.format->codec = requestedFormat->codec;

            transcodeParameters.outputParameters.stripMetadata = false; // We want clients to use metadata (offline use, replay gain, etc.)

            LMS_LOG(API_SUBSONIC, DEBUG, "Transcoding to format '" << requestedFormat->name << "'" << (bitrate ? (" with bitrate " + std::to_string(*bitrate) + " bps") : ""));

            return parameters;
        }
    } // namespace

    Response handleGetLyrics(RequestContext& context)
    {
        std::string artistName{ getParameterAs<std::string>(context.getParameters(), "artist").value_or("") };
        std::string titleName{ getParameterAs<std::string>(context.getParameters(), "title").value_or("") };

        Response response{ Response::createOkResponse() };

        // best effort search, as this API is really limited
        auto transaction{ context.getDbSession().createReadTransaction() };

        db::Track::FindParameters params;
        params.setName(titleName);
        params.setArtistName(artistName);
        params.setRange(db::Range{ .offset = 0, .size = 2 });

        // Choice: we return nothing if there are too many results
        const auto tracks{ db::Track::findIds(context.getDbSession(), params) };
        if (tracks.size() == 1)
        {
            // Choice: we return only the first lyrics if the track has many lyrics
            db::TrackLyrics::FindParameters lyricsParams;
            lyricsParams.setTrack(tracks[0]);
            lyricsParams.setSortMethod(db::TrackLyricsSortMethod::ExternalFirst);
            lyricsParams.setRange(db::Range{ 0, 1 });

            db::TrackLyrics::find(context.getDbSession(), lyricsParams, [&](const db::TrackLyrics::pointer& lyrics) {
                response.addNode("lyrics", createLyricsNode(context, lyrics));
            });
        }

        return response;
    }

    Response handleGetLyricsBySongId(RequestContext& context)
    {
        // mandatory params
        db::TrackId id{ getMandatoryParameterAs<db::TrackId>(context.getParameters(), "id") };

        Response response{ Response::createOkResponse() };
        Response::Node& lyricsList{ response.createNode("lyricsList") };
        lyricsList.createEmptyArrayChild("structuredLyrics");

        auto transaction{ context.getDbSession().createReadTransaction() };
        const db::Track::pointer track{ db::Track::find(context.getDbSession(), id) };
        if (track)
        {
            db::TrackLyrics::FindParameters params;
            params.setTrack(track->getId());
            params.setExternal(true); // First try to only report external lyrics as they are often duplicate of embedded lyrics and support more features

            bool hasExternalLyrics{};
            db::TrackLyrics::find(context.getDbSession(), params, [&](const db::TrackLyrics::pointer& lyrics) {
                lyricsList.addArrayChild("structuredLyrics", createStructuredLyricsNode(context, lyrics));
                hasExternalLyrics = true;
            });

            if (!hasExternalLyrics)
            {
                params.setExternal(false);
                db::TrackLyrics::find(context.getDbSession(), params, [&](const db::TrackLyrics::pointer& lyrics) {
                    lyricsList.addArrayChild("structuredLyrics", createStructuredLyricsNode(context, lyrics));
                });
            }
        }

        return response;
    }

    void handleDownload(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        std::shared_ptr<core::IResourceHandler> resourceHandler;

        Wt::Http::ResponseContinuation* continuation{ request.continuation() };
        if (!continuation)
        {
            // Mandatory params
            db::TrackId id{ getMandatoryParameterAs<db::TrackId>(context.getParameters(), "id") };

            std::filesystem::path trackPath;
            {
                auto transaction{ context.getDbSession().createReadTransaction() };

                auto track{ db::Track::find(context.getDbSession(), id) };
                if (!track)
                    throw RequestedDataNotFoundError{};

                trackPath = track->getAbsoluteFilePath();
            }

            resourceHandler = core::createFileResourceHandler(trackPath);
        }
        else
        {
            resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<core::IResourceHandler>>(continuation->data());
        }

        continuation = resourceHandler->processRequest(request, response);
        if (continuation)
            continuation->setData(resourceHandler);
    }

    void handleStream(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        std::shared_ptr<core::IResourceHandler> resourceHandler;

        Wt::Http::ResponseContinuation* continuation = request.continuation();
        if (!continuation)
        {
            StreamParameters streamParameters{ getStreamParameters(context) };
            if (streamParameters.transcodeParameters)
                resourceHandler = core::Service<transcoding::ITranscodeService>::get()->createTranscodeResourceHandler(*streamParameters.transcodeParameters, streamParameters.estimateContentLength);
            else
                resourceHandler = core::createFileResourceHandler(streamParameters.filePath, core::media::getMimeType(streamParameters.audioProperties.container, streamParameters.audioProperties.codec).str());
        }
        else
        {
            resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<core::IResourceHandler>>(continuation->data());
        }

        continuation = resourceHandler->processRequest(request, response);
        if (continuation)
            continuation->setData(resourceHandler);
    }

    void handleGetCoverArt(RequestContext& context, const Wt::Http::Request& /*request*/, Wt::Http::Response& response)
    {
        // Mandatory params
        const db::ArtworkId artworkId{ getMandatoryParameterAs<db::ArtworkId>(context.getParameters(), "id") };

        std::optional<std::size_t> size{ getParameterAs<std::size_t>(context.getParameters(), "size") };
        if (size)
            *size = std::clamp(*size, std::size_t{ 32 }, std::size_t{ 2048 });

        std::shared_ptr<image::IEncodedImage> image{ core::Service<artwork::IArtworkService>::get()->getImage(artworkId, size) };
        if (!image)
        {
            response.setStatus(404);
            return;
        }

        response.out().write(reinterpret_cast<const char*>(image->getData().data()), image->getData().size());
        response.setMimeType(std::string{ image->getMimeType() });
    }

} // namespace lms::api::subsonic
