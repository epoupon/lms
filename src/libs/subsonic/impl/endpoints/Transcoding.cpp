/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "Transcoding.hpp"

#include <chrono>
#include <memory>

#include "core/IResourceHandler.hpp"
#include "core/Service.hpp"
#include "core/UUID.hpp"
#include "core/Utils.hpp"

#include "database/objects/PodcastEpisodeId.hpp"
#include "services/transcoding/ITranscodeService.hpp"

#include "ParameterParsing.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"
#include "payloads/ClientInfo.hpp"
#include "payloads/StreamDetails.hpp"
#include "transcoding/AudioFileInfo.hpp"
#include "transcoding/TranscodeDecision.hpp"
#include "transcoding/TranscodeDecisionTracker.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        StreamDetails createStreamDetailsFromAudioProperties(const audio::AudioProperties& audioProperties)
        {
            StreamDetails res;
            res.protocol = "http";
            res.container = core::media::containerTypeToString(audioProperties.container).str();
            res.codec = core::media::codecTypeToString(audioProperties.codec).str();
            res.audioChannels = audioProperties.channelCount;
            res.audioBitrate = audioProperties.bitrate;
            res.audioProfile = ""; // TODO
            res.audioSamplerate = audioProperties.sampleRate;
            res.audioBitdepth = audioProperties.bitsPerSample;

            return res;
        }

        AudioFileId getMandatoryMediaIdParameter(RequestContext& context)
        {
            const std::string mediaType{ getMandatoryParameterAs<std::string>(context.getParameters(), "mediaType") };

            AudioFileId audioFileId;
            if (mediaType == "song")
                audioFileId = getMandatoryParameterAs<db::TrackId>(context.getParameters(), "mediaId");
            else if (mediaType == "podcast")
                audioFileId = getMandatoryParameterAs<db::PodcastEpisodeId>(context.getParameters(), "mediaId");
            else
                throw BadParameterGenericError{ "id", "must be 'song' or 'podcast'" };

            return audioFileId;
        }

    } // namespace

    Response handleGetTranscodeDecision(RequestContext& context)
    {
        // Parameters
        const AudioFileId audioFileId{ getMandatoryMediaIdParameter(context) };

        const ClientInfo clientInfo{ parseClientInfoFromJson(context.getBody()) };
        const AudioFileInfo audioFileInfo{ getAudioFileInfo(context.getDbSession(), audioFileId) };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node& transcodeNode{ response.createNode("transcodeDecision") };

        {
            const StreamDetails sourceStream{ createStreamDetailsFromAudioProperties(audioFileInfo.audioProperties) };
            transcodeNode.addChild("sourceStream", createStreamDetails(sourceStream));
        }

        const details::TranscodeDecisionResult transcodeDecision{ details::computeTranscodeDecision(clientInfo, audioFileInfo.audioProperties) };

        std::visit(core::utils::overloads{
                       [&](const details::DirectPlayResult&) {
                           transcodeNode.setAttribute("canDirectPlay", true);
                           transcodeNode.setAttribute("canTranscode", false);
                       },
                       [&](const details::TranscodeResult& transcodeRes) {
                           transcodeNode.setAttribute("canDirectPlay", false);
                           transcodeNode.setAttribute("canTranscode", true);

                           for (details::TranscodeReason reason : transcodeRes.reasons)
                               transcodeNode.addArrayValue("transcodeReason", transcodeReasonToString(reason).str());

                           const core::UUID uuid{ getTranscodeDecisionTracker().add(audioFileId, transcodeRes.targetStreamInfo) };
                           transcodeNode.addChild("transcodeStream", createStreamDetails(transcodeRes.targetStreamInfo));
                           transcodeNode.setAttribute("transcodeParams", uuid.getAsString());
                       },
                       [&](const details::FailureResult& failureRes) {
                           transcodeNode.setAttribute("canDirectPlay", false);
                           transcodeNode.setAttribute("canTranscode", false);
                           transcodeNode.setAttribute("errorReason", failureRes.reason);
                       } },
                   transcodeDecision);

        return response;
    }

    audio::TranscodeParameters getTranscodingParameters(RequestContext& context)
    {
        // Parameters
        const AudioFileId audioFileId{ getMandatoryMediaIdParameter(context) };
        const core::UUID uuid{ getMandatoryParameterAs<core::UUID>(context.getParameters(), "transcodeParams") };
        const std::chrono::seconds offset{ getParameterAs<std::size_t>(context.getParameters(), "offset").value_or(0) };

        const std::shared_ptr<ITranscodeDecisionTracker::Entry> entry{ getTranscodeDecisionTracker().get(uuid) };
        if (!entry || entry->audioFileId != audioFileId)
            throw RequestedDataNotFoundError{};

        const AudioFileInfo audioFileInfo{ getAudioFileInfo(context.getDbSession(), audioFileId) };

        audio::TranscodeParameters params;
        params.inputParameters.filePath = audioFileInfo.path;
        params.inputParameters.audioProperties = audioFileInfo.audioProperties;
        params.inputParameters.offset = offset;

        if (entry->targetStreamInfo.audioChannels)
            params.outputParameters.channelCount = *entry->targetStreamInfo.audioChannels;
        if (entry->targetStreamInfo.audioSamplerate)
            params.outputParameters.sampleRate = *entry->targetStreamInfo.audioSamplerate;
        if (entry->targetStreamInfo.audioBitrate)
            params.outputParameters.bitrate = *entry->targetStreamInfo.audioBitrate;
        if (entry->targetStreamInfo.audioBitdepth)
            params.outputParameters.bitsPerSample = *entry->targetStreamInfo.audioBitdepth;

        params.outputParameters.stripMetadata = false;

        const audio::TranscodeOutputFormat* transcodeOutputFormat{ details::selectTranscodeOutputFormat(entry->targetStreamInfo.container, entry->targetStreamInfo.codec) };
        if (!transcodeOutputFormat)
            throw InternalErrorGenericError{ "Unsupported output format" };

        params.outputParameters.format = *transcodeOutputFormat;

        return params;
    }

    void handleGetTranscodeStream(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        std::shared_ptr<core::IResourceHandler> resourceHandler;

        Wt::Http::ResponseContinuation* continuation = request.continuation();
        if (!continuation)
        {
            const audio::TranscodeParameters params{ getTranscodingParameters(context) };
            resourceHandler = core::Service<transcoding::ITranscodeService>::get()->createTranscodeResourceHandler(params, false /* estimate content length */);
        }
        else
        {
            resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<core::IResourceHandler>>(continuation->data());
        }
        assert(resourceHandler); // handles errors internally

        continuation = resourceHandler->processRequest(request, response);
        if (continuation)
            continuation->setData(resourceHandler);
    }
} // namespace lms::api::subsonic
