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

#include "AudioFileInfo.hpp"

#include "audio/AudioProperties.hpp"
#include "audio/Exception.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "audio/IAudioFileInfoParser.hpp"

#include "database/Session.hpp"
#include "database/objects/PodcastEpisode.hpp"
#include "database/objects/Track.hpp"

#include "services/podcast/IPodcastService.hpp"

#include "SubsonicResponse.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        audio::AudioProperties getAudioProperties(const std::filesystem::path& trackPath)
        {
            try
            {
                const auto parser{ audio::createAudioFileInfoParser(audio::AudioFileInfoParserBackend::FFmpeg) };

                audio::AudioFileInfoParseOptions parseOptions;
                parseOptions.audioPropertiesReadStyle = audio::AudioFileInfoParseOptions::AudioPropertiesReadStyle::Average;
                parseOptions.readImages = false;
                parseOptions.readTags = false;
                const auto audioFile{ parser->parse(trackPath, parseOptions) };

                const audio::AudioProperties* properties{ audioFile->getAudioProperties() };
                if (!properties)
                    throw RequestedDataNotFoundError{};

                return *properties;
            }
            catch (const audio::Exception& e)
            {
                throw RequestedDataNotFoundError{};
            }
        }

    } // namespace

    AudioFileInfo getAudioFileInfo(db::Session& session, AudioFileId audioFileId)
    {
        AudioFileInfo res;

        auto transaction{ session.createReadTransaction() };

        if (const db::TrackId * trackId{ std::get_if<db::TrackId>(&audioFileId) })
        {
            const db::Track::pointer track{ db::Track::find(session, *trackId) };
            if (!track)
                throw RequestedDataNotFoundError{};

            res.path = track->getAbsoluteFilePath();
            if (track->getContainer() && track->getCodec())
            {
                res.audioProperties.container = *track->getContainer();
                res.audioProperties.codec = *track->getCodec();
                res.audioProperties.duration = track->getDuration();
                res.audioProperties.bitrate = track->getBitrate();
                res.audioProperties.channelCount = track->getChannelCount();
                res.audioProperties.sampleRate = track->getSampleRate();
                res.audioProperties.bitsPerSample = track->getBitsPerSample();
            }
            else
            {
                res.audioProperties = getAudioProperties(res.path);
            }
        }
        else if (const db::PodcastEpisodeId * episodeId{ std::get_if<db::PodcastEpisodeId>(&audioFileId) })
        {
            const db::PodcastEpisode::pointer episode{ db::PodcastEpisode::find(session, *episodeId) };
            if (!episode)
                throw RequestedDataNotFoundError{};

            std::filesystem::path podcastCachePath{ core::Service<podcast::IPodcastService>::get()->getCachePath() };

            res.path = podcastCachePath / episode->getAudioRelativeFilePath();
            if (episode->getContainer() && episode->getCodec())
            {
                res.audioProperties.container = *episode->getContainer();
                res.audioProperties.codec = *episode->getCodec();
                res.audioProperties.duration = episode->getDuration();
                res.audioProperties.bitrate = episode->getBitrate();
                res.audioProperties.channelCount = episode->getChannelCount();
                res.audioProperties.sampleRate = episode->getSampleRate();
                res.audioProperties.bitsPerSample = episode->getBitsPerSample();
            }
            else
            {
                res.audioProperties = getAudioProperties(res.path);
            }
        }

        return res;
    }
} // namespace lms::api::subsonic