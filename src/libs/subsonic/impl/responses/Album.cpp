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

#include "responses/Album.hpp"

#include "services/database/Cluster.hpp"
#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "responses/Artist.hpp"
#include "SubsonicId.hpp"

namespace API::Subsonic
{
    using namespace Database;

    Response::Node createAlbumNode(const Release::pointer& release, Session& dbSession, const User::pointer& user, bool id3)
    {
        Response::Node albumNode;

        if (id3) {
            albumNode.setAttribute("name", release->getName());
            albumNode.setAttribute("songCount", release->getTracksCount());
            albumNode.setAttribute(
                "duration", std::chrono::duration_cast<std::chrono::seconds>(
                    release->getDuration())
                .count());
        }
        else
        {
            albumNode.setAttribute("title", release->getName());
            albumNode.setAttribute("isDir", true);
        }

        albumNode.setAttribute("created", StringUtils::toISO8601String(release->getLastWritten()));
        albumNode.setAttribute("id", idToString(release->getId()));
        albumNode.setAttribute("coverArt", idToString(release->getId()));
        if (const Wt::WDate releaseDate{ release->getReleaseDate() }; releaseDate.isValid())
            albumNode.setAttribute("year", releaseDate.year());

        auto artists{ release->getReleaseArtists() };
        if (artists.empty())
            artists = release->getArtists();

        if (artists.empty() && !id3)
        {
            albumNode.setAttribute("parent", idToString(RootId{}));
        }
        else if (!artists.empty())
        {
            albumNode.setAttribute("artist", Utils::joinArtistNames(artists));

            if (artists.size() == 1)
            {
                albumNode.setAttribute(id3 ? "artistId" : "parent", idToString(artists.front()->getId()));
            }
            else
            {
                if (!id3)
                    albumNode.setAttribute("parent", idToString(RootId{}));
            }
        }

        // Report the first GENRE for this track
        ClusterType::pointer clusterType{ ClusterType::find(dbSession, "GENRE") };
        if (clusterType)
        {
            auto clusters{ release->getClusterGroups({clusterType}, 1) };
            if (!clusters.empty() && !clusters.front().empty())
                albumNode.setAttribute("genre", clusters.front().front()->getName());
        }

        if (const Wt::WDateTime dateTime{ Service<Scrobbling::IScrobblingService>::get()->getStarredDateTime(user->getId(), release->getId()) }; dateTime.isValid())
            albumNode.setAttribute("starred", StringUtils::toISO8601String(dateTime)); // TODO report correct date/time

        // OpenSubsonic specific fields (must always be set)
        {
            std::optional<UUID> mbid {release->getMBID()};
            albumNode.setAttribute("musicBrainzId", mbid ? mbid->getAsString() : "");
        }
        
        return albumNode;
    }
}