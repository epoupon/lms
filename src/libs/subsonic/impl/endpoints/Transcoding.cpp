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
#include <mutex>

#include "audio/AudioTypes.hpp"
#include "core/ILogger.hpp"
#include "core/IResourceHandler.hpp"
#include "core/Random.hpp"
#include "core/UUID.hpp"
#include "core/Utils.hpp"

#include "audio/Exception.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "services/transcoding/ITranscodingService.hpp"

#include "ParameterParsing.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"
#include "responses/ClientInfo.hpp"
#include "responses/StreamDetails.hpp"
#include "transcoding/TranscodeDecision.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        class TranscodeDecisionManager
        {
        public:
            using Clock = std::chrono::steady_clock;

            struct Entry
            {
                Clock::time_point addedTimePoint;
                db::TrackId track;
                StreamDetails targetStreamInfo;
            };

            core::UUID add(db::TrackId trackId, const StreamDetails& targetStreamInfo)
            {
                const core::UUID uuid{ core::UUID::generate() };
                const Clock::time_point now{ Clock::now() };

                auto entry{ std::make_shared<Entry>(now, trackId, targetStreamInfo) };

                {
                    std::scoped_lock lock{ mutex };

                    purgeOutdatedEntries(now);

                    entries.emplace(uuid, entry);
                }

                return uuid;
            }

            std::shared_ptr<Entry> get(const core::UUID& uuid)
            {
                const Clock::time_point now{ Clock::now() };
                std::shared_ptr<Entry> res;

                std::scoped_lock lock{ mutex };

                auto it{ entries.find(uuid) };
                if (it != entries.end())
                {
                    if (now > it->second->addedTimePoint + maxEntryDuration)
                        entries.erase(it);
                    else
                        res = it->second;
                }

                return res;
            }

        private:
            void purgeOutdatedEntries(Clock::time_point now)
            {
                std::erase_if(entries, [&](const auto& entry) { return now > entry.second->addedTimePoint + maxEntryDuration; });
                while (entries.size() > maxEntryCount)
                    entries.erase(core::random::pickRandom(entries)); // TODO kill oldest one?
            }

            std::mutex mutex;
            std::unordered_map<core::UUID, std::shared_ptr<Entry>> entries;

            static constexpr std::size_t maxEntryCount{ 1'000 };
            static constexpr std::chrono::hours maxEntryDuration{ 12 };
        };

        static TranscodeDecisionManager& getTranscodeDecisionManager()
        {
            static TranscodeDecisionManager manager;
            return manager;
        }

        StreamDetails createStreamDetailsFromAudioProperties(const audio::AudioProperties& audioProperties)
        {
            // Assume source always have these elements set
            assert(audioProperties.container);
            assert(audioProperties.codec);
            assert(audioProperties.bitrate);
            assert(audioProperties.channelCount);

            StreamDetails res;
            res.protocol = "http";
            res.container = audio::containerTypeToString(*audioProperties.container).str();
            res.codec = audio::codecTypeToString(*audioProperties.codec).str();
            res.audioChannels = audioProperties.channelCount;
            res.audioBitrate = audioProperties.bitrate;
            res.audioProfile = ""; // TODO
            res.audioSamplerate = audioProperties.sampleRate;
            res.audioBitdepth = audioProperties.bitsPerSample;

            return res;
        }
    } // namespace

    Response handleGetTranscodeDecision(RequestContext& context)
    {
        // Parameters
        const db::TrackId trackId{ getMandatoryParameterAs<db::TrackId>(context.getParameters(), "songId") };
        const ClientInfo clientInfo{ parseClientInfoFromJson(context.getBody()) };

        auto transaction{ context.getDbSession().createReadTransaction() };
        const db::Track::pointer track{ db::Track::find(context.getDbSession(), trackId) };
        if (!track)
            throw RequestedDataNotFoundError{};

        // For now, we need to analyze the media to be able to decide if transcoding is needed or not (info not cached in DB)
        try
        {
            const auto audioFile{ audio::parseAudioFile(track->getAbsoluteFilePath()) };

            Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
            Response::Node& transcodeNode{ response.createNode("transcodeDecision") };

            const StreamDetails sourceStream{ createStreamDetailsFromAudioProperties(audioFile->getAudioProperties()) };

            transcodeNode.addChild("sourceStream", createStreamDetails(sourceStream));

            const details::TranscodeDecisionResult transcodeDecision{ details::computeTranscodeDecision(clientInfo, sourceStream) };

            std::visit(core::utils::overloads{
                           [&](const details::DirectPlayResult&) {
                               transcodeNode.setAttribute("canDirectPlay", true);
                               transcodeNode.setAttribute("canTranscode", false);
                           },
                           [&](const details::TranscodeResult& transcodeRes) {
                               transcodeNode.setAttribute("canDirectPlay", false);
                               transcodeNode.setAttribute("canTranscode", true);

                               const core::UUID uuid{ getTranscodeDecisionManager().add(trackId, transcodeRes.targetStreamInfo) };
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
        catch (const audio::Exception& e)
        {
            LMS_LOG(API_SUBSONIC, ERROR, "Cannot analyze audio file: " << e.what());
            throw InternalErrorGenericError{ "Cannot analyze audio file" };
        }
    }

    struct TranscodingParameters
    {
        transcoding::InputParameters inputParameters;
        transcoding::OutputParameters outputParameters;
    };

    TranscodingParameters getTranscodingParameters(RequestContext& context)
    {
        const db::TrackId trackId{ getMandatoryParameterAs<db::TrackId>(context.getParameters(), "trackID") };
        const core::UUID uuid{ getMandatoryParameterAs<core::UUID>(context.getParameters(), "transcodeParams") };
        const std::chrono::seconds offset{ getParameterAs<std::size_t>(context.getParameters(), "offset").value_or(0) };

        const auto entry{ getTranscodeDecisionManager().get(uuid) };
        if (!entry || entry->track != trackId)
            throw RequestedDataNotFoundError{};

        auto transaction{ context.getDbSession().createReadTransaction() };
        const db::Track::pointer track{ db::Track::find(context.getDbSession(), trackId) };
        if (!track)
            throw RequestedDataNotFoundError{};

        TranscodingParameters params;
        params.inputParameters.filePath = track->getAbsoluteFilePath();
        params.inputParameters.duration = track->getDuration();
        params.inputParameters.offset = offset;

        if (entry->targetStreamInfo.audioChannels)
            params.outputParameters.audioChannels = *entry->targetStreamInfo.audioChannels;
        if (entry->targetStreamInfo.audioSamplerate)
            params.outputParameters.sampleRate = *entry->targetStreamInfo.audioSamplerate;
        assert(entry->targetStreamInfo.audioBitrate);
        params.outputParameters.bitrate = *entry->targetStreamInfo.audioBitrate;
        params.outputParameters.stripMetadata = false;

        if (entry->targetStreamInfo.container == "mp3" && entry->targetStreamInfo.codec == "mp3")
            params.outputParameters.format = transcoding::OutputFormat::MP3;
        else if (entry->targetStreamInfo.container == "ogg" && entry->targetStreamInfo.codec == "opus")
            params.outputParameters.format = transcoding::OutputFormat::OGG_OPUS;
        else if (entry->targetStreamInfo.container == "ogg" && entry->targetStreamInfo.codec == "vorbis")
            params.outputParameters.format = transcoding::OutputFormat::OGG_VORBIS;
        else if (entry->targetStreamInfo.container == "webm" && entry->targetStreamInfo.codec == "vorbis")
            params.outputParameters.format = transcoding::OutputFormat::WEBM_VORBIS;
        else if (entry->targetStreamInfo.container == "matroska" && entry->targetStreamInfo.codec == "opus")
            params.outputParameters.format = transcoding::OutputFormat::MATROSKA_OPUS;
        else
            throw InternalErrorGenericError{ "Unsupported output format" };

        return params;
    }

    void handleGetTranscodeStream(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        std::shared_ptr<core::IResourceHandler> resourceHandler;

        Wt::Http::ResponseContinuation* continuation = request.continuation();
        if (!continuation)
        {
            const TranscodingParameters streamParameters{ getTranscodingParameters(context) };
            resourceHandler = core::Service<transcoding::ITranscodingService>::get()->createResourceHandler(streamParameters.inputParameters, streamParameters.outputParameters, false /* estimate content length */);
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
