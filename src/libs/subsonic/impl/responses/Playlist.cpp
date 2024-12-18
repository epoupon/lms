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

#include "core/String.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"

#include "CoverArtId.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    Response::Node createPlaylistNode(const db::TrackList::pointer& tracklist, db::Session& session)
    {
        Response::Node playlistNode;

        playlistNode.setAttribute("id", idToString(tracklist->getId()));
        playlistNode.setAttribute("name", tracklist->getName());
        playlistNode.setAttribute("songCount", tracklist->getCount());
        playlistNode.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(tracklist->getDuration()).count());
        playlistNode.setAttribute("public", tracklist->getVisibility() == db::TrackList::Visibility::Public);
        playlistNode.setAttribute("changed", core::stringUtils::toISO8601String(tracklist->getLastModifiedDateTime()));
        playlistNode.setAttribute("created", core::stringUtils::toISO8601String(tracklist->getCreationDateTime()));
        if (const db::User::pointer user{ tracklist->getUser() })
            playlistNode.setAttribute("owner", user->getLoginName());

        db::Track::FindParameters params;
        params.setTrackList(tracklist->getId());
        params.setHasEmbeddedImage(true);
        params.setRange(db::Range{ 0, 1 });
        params.setSortMethod(db::TrackSortMethod::TrackList);

        db::Track::find(session, params, [&](const db::Track::pointer& track) {
            const CoverArtId coverArtId{ track->getId(), track->getLastWriteTime().toTime_t() };
            playlistNode.setAttribute("coverArt", idToString(coverArtId));
        });

        return playlistNode;
    }
} // namespace lms::api::subsonic