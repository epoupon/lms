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
#include "av/RawResourceHandlerCreator.hpp"
#include "av/TranscodingParameters.hpp"
#include "av/TranscodingResourceHandlerCreator.hpp"
#include "av/Types.hpp"
#include "core/FileResourceHandlerCreator.hpp"
#include "core/ILogger.hpp"
#include "core/IResourceHandler.hpp"
#include "core/String.hpp"
#include "core/Utils.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackLyrics.hpp"
#include "database/User.hpp"
#include "services/artwork/IArtworkService.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "responses/Lyrics.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace
    {
        std::optional<av::transcoding::OutputFormat> subsonicStreamFormatToAvOutputFormat(std::string_view format)
        {
            for (const auto& [str, avFormat] : std::initializer_list<std::pair<std::string_view, av::transcoding::OutputFormat>>{
                     { "mp3", av::transcoding::OutputFormat::MP3 },
                     { "opus", av::transcoding::OutputFormat::OGG_OPUS },
                     { "vorbis", av::transcoding::OutputFormat::OGG_VORBIS },
                 })
            {
                if (core::stringUtils::stringCaseInsensitiveEqual(str, format))
                    return avFormat;
            }
            return std::nullopt;
        }

        av::transcoding::OutputFormat userTranscodeFormatToAvFormat(db::TranscodingOutputFormat format)
        {
            switch (format)
            {
            case db::TranscodingOutputFormat::MP3:
                return av::transcoding::OutputFormat::MP3;
            case db::TranscodingOutputFormat::OGG_OPUS:
                return av::transcoding::OutputFormat::OGG_OPUS;
            case db::TranscodingOutputFormat::MATROSKA_OPUS:
                return av::transcoding::OutputFormat::MATROSKA_OPUS;
            case db::TranscodingOutputFormat::OGG_VORBIS:
                return av::transcoding::OutputFormat::OGG_VORBIS;
            case db::TranscodingOutputFormat::WEBM_VORBIS:
                return av::transcoding::OutputFormat::WEBM_VORBIS;
            }
            return av::transcoding::OutputFormat::OGG_OPUS;
        }

        bool isCodecCompatibleWithOutputFormat(av::DecodingCodec codec, av::transcoding::OutputFormat outputFormat)
        {
            switch (outputFormat)
            {
            case av::transcoding::OutputFormat::MP3:
                return codec == av::DecodingCodec::MP3;

            case av::transcoding::OutputFormat::OGG_OPUS:
            case av::transcoding::OutputFormat::MATROSKA_OPUS:
                return codec == av::DecodingCodec::OPUS;

            case av::transcoding::OutputFormat::OGG_VORBIS:
            case av::transcoding::OutputFormat::WEBM_VORBIS:
                return codec == av::DecodingCodec::VORBIS;
            }

            return true;
        }

        struct StreamParameters
        {
            av::transcoding::InputParameters inputParameters;
            std::optional<av::transcoding::OutputParameters> outputParameters;
            bool estimateContentLength{};
        };

        bool isOutputFormatCompatible(const std::filesystem::path& trackPath, av::transcoding::OutputFormat outputFormat)
        {
            try
            {
                const auto audioFile{ av::parseAudioFile(trackPath) };

                const auto streamInfo{ audioFile->getBestStreamInfo() };
                if (!streamInfo)
                    throw RequestedDataNotFoundError{}; // TODO 404?

                return isCodecCompatibleWithOutputFormat(streamInfo->codec, outputFormat);
            }
            catch (const av::Exception& e)
            {
                // TODO 404?
                throw RequestedDataNotFoundError{};
            }
        }

        StreamParameters getStreamParameters(RequestContext& context)
        {
            // Mandatory params
            const TrackId id{ getMandatoryParameterAs<TrackId>(context.parameters, "id") };

            // Optional params
            std::size_t maxBitRate{ getParameterAs<std::size_t>(context.parameters, "maxBitRate").value_or(0) * 1000 }; // "If set to zero, no limit is imposed", given in kpbs
            const std::string format{ getParameterAs<std::string>(context.parameters, "format").value_or("") };
            std::size_t timeOffset{ getParameterAs<std::size_t>(context.parameters, "timeOffset").value_or(0) };
            bool estimateContentLength{ getParameterAs<bool>(context.parameters, "estimateContentLength").value_or(false) };

            StreamParameters parameters;

            auto transaction{ context.dbSession.createReadTransaction() };

            const auto track{ Track::find(context.dbSession, id) };
            if (!track)
                throw RequestedDataNotFoundError{};

            parameters.inputParameters.trackPath = track->getAbsoluteFilePath();
            parameters.inputParameters.duration = track->getDuration();
            parameters.estimateContentLength = estimateContentLength;

            if (format == "raw") // raw => no transcoding
                return parameters;

            std::optional<av::transcoding::OutputFormat> requestedFormat{ subsonicStreamFormatToAvOutputFormat(format) };
            if (!requestedFormat)
            {
                if (context.user->getSubsonicEnableTranscodingByDefault())
                    requestedFormat = userTranscodeFormatToAvFormat(context.user->getSubsonicDefaultTranscodingOutputFormat());
            }

            if (!requestedFormat && (maxBitRate == 0 || track->getBitrate() <= maxBitRate))
            {
                LMS_LOG(API_SUBSONIC, DEBUG, "File's bitrate is compatible with parameters => no transcoding");
                return parameters; // no transcoding needed
            }

            // scan the file to check if its format is compatible with the actual requested format
            //  same codec => apply max bitrate
            //  otherwise => apply default bitrate (because we can't really compare bitrates between formats) + max bitrate)
            std::size_t bitrate{};
            if (requestedFormat && isOutputFormatCompatible(track->getAbsoluteFilePath(), *requestedFormat))
            {
                if (maxBitRate == 0 || track->getBitrate() <= maxBitRate)
                {
                    LMS_LOG(API_SUBSONIC, DEBUG, "File's bitrate and format are compatible with parameters => no transcoding");
                    return parameters; // no transcoding needed
                }
                bitrate = maxBitRate;
            }

            if (!requestedFormat)
                requestedFormat = userTranscodeFormatToAvFormat(context.user->getSubsonicDefaultTranscodingOutputFormat());
            if (!bitrate)
                bitrate = std::min<std::size_t>(context.user->getSubsonicDefaultTranscodingOutputBitrate(), maxBitRate);

            av::transcoding::OutputParameters& outputParameters{ parameters.outputParameters.emplace() };

            outputParameters.stripMetadata = false; // We want clients to use metadata (offline use, replay gain, etc.)
            outputParameters.offset = std::chrono::seconds{ timeOffset };
            outputParameters.format = *requestedFormat;
            outputParameters.bitrate = bitrate;

            return parameters;
        }
    } // namespace

    Response handleGetLyrics(RequestContext& context)
    {
        std::string artistName{ getParameterAs<std::string>(context.parameters, "artist").value_or("") };
        std::string titleName{ getParameterAs<std::string>(context.parameters, "title").value_or("") };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };

        // best effort search, as this API is really limited
        auto transaction{ context.dbSession.createReadTransaction() };

        db::Track::FindParameters params;
        params.name = titleName;
        params.artistName = artistName;
        params.range = Range{ 0, 2 };

        // Choice: we return nothing if there are too many results
        const auto tracks{ db::Track::findIds(context.dbSession, params) };
        if (tracks.results.size() == 1)
        {
            // Choice: we return only the first lyrics if the track has many lyrics
            db::TrackLyrics::FindParameters lyricsParams;
            lyricsParams.setTrack(tracks.results[0]);
            lyricsParams.setSortMethod(TrackLyricsSortMethod::ExternalFirst);
            lyricsParams.setRange(db::Range{ 0, 1 });

            db::TrackLyrics::find(context.dbSession, lyricsParams, [&](const db::TrackLyrics::pointer& lyrics) {
                response.addNode("lyrics", createLyricsNode(context, lyrics));
            });
        }

        return response;
    }

    Response handleGetLyricsBySongId(RequestContext& context)
    {
        // mandatory params
        db::TrackId id{ getMandatoryParameterAs<db::TrackId>(context.parameters, "id") };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& lyricsList{ response.createNode("lyricsList") };
        lyricsList.createEmptyArrayChild("structuredLyrics");

        auto transaction{ context.dbSession.createReadTransaction() };
        const db::Track::pointer track{ db::Track::find(context.dbSession, id) };
        if (track)
        {
            db::TrackLyrics::FindParameters params;
            params.setTrack(track->getId());
            params.setExternal(true); // First try to only report external lyrics as they are often duplicate of embedded lyrics and support more features

            bool hasExternalLyrics{};
            db::TrackLyrics::find(context.dbSession, params, [&](const db::TrackLyrics::pointer& lyrics) {
                lyricsList.addArrayChild("structuredLyrics", createStructuredLyricsNode(context, lyrics));
                hasExternalLyrics = true;
            });

            if (!hasExternalLyrics)
            {
                params.setExternal(false);
                db::TrackLyrics::find(context.dbSession, params, [&](const db::TrackLyrics::pointer& lyrics) {
                    lyricsList.addArrayChild("structuredLyrics", createStructuredLyricsNode(context, lyrics));
                });
            }
        }

        return response;
    }

    void handleDownload(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        std::shared_ptr<IResourceHandler> resourceHandler;

        Wt::Http::ResponseContinuation* continuation{ request.continuation() };
        if (!continuation)
        {
            // Mandatory params
            db::TrackId id{ getMandatoryParameterAs<db::TrackId>(context.parameters, "id") };

            std::filesystem::path trackPath;
            {
                auto transaction{ context.dbSession.createReadTransaction() };

                auto track{ Track::find(context.dbSession, id) };
                if (!track)
                    throw RequestedDataNotFoundError{};

                trackPath = track->getAbsoluteFilePath();
            }

            resourceHandler = av::createRawResourceHandler(trackPath);
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
                if (streamParameters.outputParameters)
                    resourceHandler = av::transcoding::createResourceHandler(streamParameters.inputParameters, *streamParameters.outputParameters, streamParameters.estimateContentLength);
                else
                    resourceHandler = av::createRawResourceHandler(streamParameters.inputParameters.trackPath);
            }
            else
            {
                resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(continuation->data());
            }

            continuation = resourceHandler->processRequest(request, response);
            if (continuation)
                continuation->setData(resourceHandler);
        }
        catch (const av::Exception& e)
        {
            LMS_LOG(API_SUBSONIC, ERROR, "Caught Av exception: " << e.what());
        }
    }

    void handleGetCoverArt(RequestContext& context, const Wt::Http::Request& /*request*/, Wt::Http::Response& response)
    {
        // Mandatory params
        const auto trackId{ getParameterAs<TrackId>(context.parameters, "id") };
        const auto releaseId{ getParameterAs<ReleaseId>(context.parameters, "id") };
        const auto artistId{ getParameterAs<ArtistId>(context.parameters, "id") };

        if (!trackId && !releaseId && !artistId)
            throw BadParameterGenericError{ "id" };

        std::size_t size{ getParameterAs<std::size_t>(context.parameters, "size").value_or(1024) };
        size = core::utils::clamp(size, std::size_t{ 32 }, std::size_t{ 2048 });

        std::shared_ptr<image::IEncodedImage> cover;
        if (trackId)
            cover = core::Service<cover::IArtworkService>::get()->getTrackImage(*trackId, size);
        else if (releaseId)
            cover = core::Service<cover::IArtworkService>::get()->getReleaseCover(*releaseId, size);
        else if (artistId)
            cover = core::Service<cover::IArtworkService>::get()->getArtistImage(*artistId, size);

        if (!cover && context.enableDefaultCover && !artistId)
            cover = core::Service<cover::IArtworkService>::get()->getDefaultReleaseCover();

        if (!cover)
        {
            response.setStatus(404);
            return;
        }

        response.out().write(reinterpret_cast<const char*>(cover->getData()), cover->getDataSize());
        response.setMimeType(std::string{ cover->getMimeType() });
    }

} // namespace lms::api::subsonic
