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
#include "database/objects/Artwork.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"
#include "services/artwork/IArtworkService.hpp"

#include "CoverArtId.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    Response::Node createPlaylistNode(RequestContext& context, const db::TrackList::pointer& tracklist)
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

        if (const db::ArtworkId artworkId{ core::Service<artwork::IArtworkService>::get()->findTrackListImage(tracklist->getId()) }; artworkId.isValid())
        {
            if (const auto artwork{ db::Artwork::find(context.dbSession, artworkId) })
            {
                CoverArtId coverArtId{ artwork->getId(), artwork->getLastWrittenTime().toTime_t() };
                playlistNode.setAttribute("coverArt", idToString(coverArtId));
            }
        }

        return playlistNode;
    }
} // namespace lms::api::subsonic