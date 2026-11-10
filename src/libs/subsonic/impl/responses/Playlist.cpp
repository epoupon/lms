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

#include "core/Service.hpp"
#include "core/String.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"
#include "services/artwork/IArtworkService.hpp"

#include "RequestContext.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    Response::Node createPlaylistNode(RequestContext& context, const db::TrackList& tracklist)
    {
        Response::Node playlistNode;

        playlistNode.setAttribute("id", idToString(tracklist.getId()));
        playlistNode.setAttribute("name", tracklist.getName());
        playlistNode.setAttribute("songCount", db::TrackList::getTrackCount(context.getDbSession(), tracklist.getId()));
        playlistNode.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(db::TrackList::getDuration(context.getDbSession(), tracklist.getId())).count());
        playlistNode.setAttribute("public", tracklist.getVisibility() == db::TrackList::Visibility::Public);
        playlistNode.setAttribute("changed", core::stringUtils::toISO8601String(tracklist.getLastModifiedDateTime()));
        playlistNode.setAttribute("created", core::stringUtils::toISO8601String(tracklist.getCreationDateTime()));

        const std::optional<db::User> trackListUser{ tracklist.getUserId().isValid() ? db::User::find(context.getDbSession(), tracklist.getUserId()) : std::nullopt };
        if (trackListUser)
            playlistNode.setAttribute("owner", trackListUser->getLoginName());

        if (const db::ArtworkId artworkId{ core::Service<artwork::IArtworkService>::get()->findTrackListImage(tracklist.getId()) }; artworkId.isValid())
            playlistNode.setAttribute("coverArt", idToString(artworkId));

        if (context.isOpenSubsonicEnabled())
        {
            // A playlist with no owner is always read only, and only the owner of a tracklist can modify it
            playlistNode.setAttribute("readonly", !trackListUser || trackListUser->getId() != context.getUser()->getId());
        }

        return playlistNode;
    }
} // namespace lms::api::subsonic