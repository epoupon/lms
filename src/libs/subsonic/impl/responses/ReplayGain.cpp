/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "responses/ReplayGain.hpp"

#include "database/objects/Medium.hpp"
#include "database/objects/Track.hpp"

namespace lms::api::subsonic
{
    Response::Node createReplayGainNode(const db::Track::pointer& track, const db::Medium::pointer& medium)
    {
        Response::Node replayGainNode;

        if (const auto trackReplayGain{ track->getReplayGain() })
            replayGainNode.setAttribute("trackGain", *trackReplayGain);

        if (medium)
        {
            if (const auto releaseReplayGain{ medium->getReplayGain() })
                replayGainNode.setAttribute("albumGain", *releaseReplayGain);
        }

        return replayGainNode;
    }
} // namespace lms::api::subsonic