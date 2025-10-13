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

#include "StreamDetails.hpp"

#include "av/IAudioFile.hpp"

namespace lms::api::subsonic
{
    Response::Node createStreamDetails(const av::ContainerInfo& containerInfo, const av::StreamInfo& streamInfo)
    {
        Response::Node streamDetailsNode{};

        streamDetailsNode.setAttribute("protocol", "http");
        streamDetailsNode.setAttribute("container", containerInfo.name);
        streamDetailsNode.setAttribute("codec", streamInfo.codecName);
        if (streamInfo.channelCount > 0)
            streamDetailsNode.setAttribute("audioChannels", streamInfo.channelCount);
        if (streamInfo.bitrate > 0)
            streamDetailsNode.setAttribute("audioBitrate", streamInfo.bitrate);
        // TODO audioProfile
        if (streamInfo.sampleRate > 0)
            streamDetailsNode.setAttribute("audioSamplerate", streamInfo.sampleRate);
        if (streamInfo.bitsPerSample > 0)
            streamDetailsNode.setAttribute("audioBitdepth", streamInfo.bitsPerSample);

        return streamDetailsNode;
    }
} // namespace lms::api::subsonic