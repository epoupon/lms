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

#include "Playlist.hpp"

#include "services/database/TrackList.hpp"
#include "services/database/User.hpp"
#include "SubsonicId.hpp"

namespace API::Subsonic
{
    using namespace Database;

    static const std::string_view reportedDummyDate{ "2000-01-01T00:00:00" };

    Response::Node createPlaylistNode(const TrackList::pointer& tracklist, Session&)
    {
        Response::Node playlistNode;

        playlistNode.setAttribute("id", idToString(tracklist->getId()));
        playlistNode.setAttribute("name", tracklist->getName());
        playlistNode.setAttribute("songCount", tracklist->getCount());
        playlistNode.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(tracklist->getDuration()).count());
        playlistNode.setAttribute("public", tracklist->isPublic());
        playlistNode.setAttribute("created", reportedDummyDate);
        playlistNode.setAttribute("owner", tracklist->getUser()->getLoginName());

        return playlistNode;
    }
}