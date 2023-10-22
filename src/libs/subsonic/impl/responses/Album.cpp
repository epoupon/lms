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
#include "responses/DiscTitle.hpp"
#include "responses/ItemGenre.hpp"
#include "SubsonicId.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace
    {
        std::string_view toString(ReleaseTypePrimary releaseType)
        {
            switch (releaseType)
            {
            case ReleaseTypePrimary::Album: return "album";
            case ReleaseTypePrimary::Broadcast: return "broadcast";
            case ReleaseTypePrimary::EP: return "ep";
            case ReleaseTypePrimary::Single: return "single";
            case ReleaseTypePrimary::Other: return "other";
            }

            return "unknown";
        }

        std::string_view toString(ReleaseTypeSecondary releaseType)
        {
            switch (releaseType)
            {
            case ReleaseTypeSecondary::Audiobook: return "audiobook";
            case ReleaseTypeSecondary::AudioDrama: return "audiodrama";
            case ReleaseTypeSecondary::Compilation: return "compilation";
            case ReleaseTypeSecondary::Demo: return "demo";
            case ReleaseTypeSecondary::DJMix: return "djmix";
            case ReleaseTypeSecondary::Interview: return "interview";
            case ReleaseTypeSecondary::Live: return "live";
            case ReleaseTypeSecondary::Mixtape_Street: return "mixtapestreet";
            case ReleaseTypeSecondary::Remix: return "remix";
            case ReleaseTypeSecondary::Soundtrack: return "soundtrack";
            case ReleaseTypeSecondary::Spokenword: return "soundtrack";
            }

            return "unknown";
        }
    }

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
            if (!release->getArtistDisplayName().empty())
                albumNode.setAttribute("artist", release->getArtistDisplayName());
            else
                albumNode.setAttribute("artist", Utils::joinArtistNames(artists));

            if (artists.size() == 1)
            {
                albumNode.setAttribute(id3 ? Response::Node::Key{ "artistId" } : Response::Node::Key{ "parent" }, idToString(artists.front()->getId()));
            }
            else
            {
                if (!id3)
                    albumNode.setAttribute("parent", idToString(RootId{}));
            }
        }

        // Report the first GENRE for this track
        const ClusterType::pointer genreClusterType{ ClusterType::find(dbSession, "GENRE") };
        if (genreClusterType)
        {
            auto clusters{ release->getClusterGroups({genreClusterType}, 1) };
            if (!clusters.empty() && !clusters.front().empty())
                albumNode.setAttribute("genre", clusters.front().front()->getName());
        }

        if (const Wt::WDateTime dateTime{ Service<Scrobbling::IScrobblingService>::get()->getStarredDateTime(user->getId(), release->getId()) }; dateTime.isValid())
            albumNode.setAttribute("starred", StringUtils::toISO8601String(dateTime));

        // OpenSubsonic specific fields (must always be set)
        if (!id3)
            albumNode.setAttribute("mediaType", "album");

        {
            const Wt::WDateTime dateTime{ Service<Scrobbling::IScrobblingService>::get()->getLastListenDateTime(user->getId(), release->getId()) };
            albumNode.setAttribute("played", dateTime.isValid() ? StringUtils::toISO8601String(dateTime) : std::string{ "" });
        }

        {
            std::optional<UUID> mbid{ release->getMBID() };
            albumNode.setAttribute("musicBrainzId", mbid ? mbid->getAsString() : "");
        }

        auto addClusters{ [&](Response::Node::Key field, std::string_view clusterTypeName)
        {
            albumNode.createEmptyArrayValue(field);

            ClusterType::pointer clusterType{ ClusterType::find(dbSession, clusterTypeName) };
            if (clusterType)
            {
                Cluster::FindParameters params;
                params.setRelease(release->getId());
                params.setClusterType(clusterType->getId());

                for (const auto& cluster : Cluster::find(dbSession, params).results)
                    albumNode.addArrayValue(field, std::get<std::string>(cluster));
            }
        } };

        addClusters("moods", "MOOD");

        // Genres
        albumNode.createEmptyArrayChild("genres");
        if (genreClusterType)
        {
            Cluster::FindParameters params;
            params.setRelease(release->getId());
            params.setClusterType(genreClusterType->getId());

            for (const auto& cluster : Cluster::find(dbSession, params).results)
                albumNode.addArrayChild("genres", createItemGenreNode(std::get<std::string>(cluster)));
        }

        albumNode.createEmptyArrayChild("artists");
        for (const Artist::pointer& artist : release->getReleaseArtists())
            albumNode.addArrayChild("artists", createArtistNode(artist));

        albumNode.setAttribute("displayArtist", release->getArtistDisplayName());

        {
            const Wt::WDate originalReleaseDate{ release->getOriginalReleaseDate() };
            albumNode.setAttribute("originalReleaseDate", originalReleaseDate.isValid() ? StringUtils::toISO8601String(originalReleaseDate) : "");
        }

        albumNode.setAttribute("isCompilation", release->getSecondaryTypes().contains(ReleaseTypeSecondary::Compilation));
        albumNode.createEmptyArrayValue("releaseTypes");
        if (auto releaseType{ release->getPrimaryType() })
            albumNode.addArrayValue("releaseTypes", toString(*releaseType));
        for (const ReleaseTypeSecondary releaseType : release->getSecondaryTypes())
            albumNode.addArrayValue("releaseTypes", toString(releaseType));

        // disc titles
        albumNode.createEmptyArrayChild("discTitles");
        for (const DiscInfo& discInfo : release->getDiscs())
        {
            if (!discInfo.name.empty())
                albumNode.addArrayChild("discTitles", createDiscTitle(discInfo));
        }

        return albumNode;
    }
}