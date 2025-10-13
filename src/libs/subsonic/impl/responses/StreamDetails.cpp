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

namespace lms::api::subsonic
{
    Response::Node createStreamDetails(const StreamDetails& streamDetails)
    {
        Response::Node streamDetailsNode{};

        streamDetailsNode.setAttribute("protocol", "http");
        streamDetailsNode.setAttribute("container", streamDetails.container);
        streamDetailsNode.setAttribute("codec", streamDetails.codec);
        if (streamDetails.audioChannels)
            streamDetailsNode.setAttribute("audioChannels", *streamDetails.audioChannels);
        if (streamDetails.audioBitrate)
            streamDetailsNode.setAttribute("audioBitrate", *streamDetails.audioBitrate);
        if (!streamDetails.audioProfile.empty())
            streamDetailsNode.setAttribute("audioProfile", streamDetails.audioProfile);
        if (streamDetails.audioSamplerate)
            streamDetailsNode.setAttribute("audioSamplerate", *streamDetails.audioSamplerate);
        if (streamDetails.audioBitdepth)
            streamDetailsNode.setAttribute("audioBitdepth", *streamDetails.audioBitdepth);

        return streamDetailsNode;
    }
} // namespace lms::api::subsonic