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

#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "CoverArtId.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "responses/Artist.hpp"
#include "responses/DiscTitle.hpp"
#include "responses/ItemDate.hpp"
#include "responses/ItemGenre.hpp"
#include "responses/RecordLabel.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    Response::Node createAlbumNode(RequestContext& context, const Release::pointer& release, bool id3, const Directory::pointer& directory)
    {
        LMS_SCOPED_TRACE_DETAILED("Subsonic", "CreateAlbum");

        Response::Node albumNode;

        if (id3)
        {
            albumNode.setAttribute("id", idToString(release->getId()));
            albumNode.setAttribute("name", release->getName());
            albumNode.setAttribute("songCount", release->getTrackCount());
            albumNode.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(release->getDuration()).count());
        }
        else
        {
            Directory::pointer directoryToReport{ directory };

            if (!directoryToReport)
            {
                Directory::FindParameters params;
                params.setRelease(release->getId());
                params.setRange(Range{ 0, 1 }); // only support 1 directory <-> 1 release
                Directory::find(context.getDbSession(), params, [&](const Directory::pointer& foundDirectory) {
                    directoryToReport = foundDirectory;
                });
            }

            if (directoryToReport)
            {
                albumNode.setAttribute("title", directoryToReport->getName());
                albumNode.setAttribute("id", idToString(directoryToReport->getId()));
                if (const Directory::pointer & parentDirectory{ directoryToReport->getParentDirectory() })
                    albumNode.setAttribute("parent", idToString(parentDirectory->getId()));
            }

            albumNode.setAttribute("album", release->getName());
            albumNode.setAttribute("isDir", true);
        }

        albumNode.setAttribute("created", core::stringUtils::toISO8601String(release->getAddedTime()));

        if (const auto artwork{ release->getPreferredArtwork() })
        {
            CoverArtId coverArtId{ artwork->getId(), artwork->getLastWrittenTime().toTime_t() };
            albumNode.setAttribute("coverArt", idToString(coverArtId));
        }

        if (const auto originalYear{ release->getOriginalYear() })
            albumNode.setAttribute("year", *originalYear);
        else if (const auto year{ release->getYear() })
            albumNode.setAttribute("year", *year);

        auto artists{ release->getReleaseArtists() };
        if (artists.empty())
            artists = release->getArtists();

        if (!artists.empty())
        {
            if (!release->getArtistDisplayName().empty())
                albumNode.setAttribute("artist", release->getArtistDisplayName());
            else
                albumNode.setAttribute("artist", utils::joinArtistNames(artists));

            if (artists.size() == 1)
            {
                albumNode.setAttribute("artistId", idToString(artists.front()->getId()));
            }
        }

        albumNode.setAttribute("playCount", core::Service<scrobbling::IScrobblingService>::get()->getCount(context.getUser()->getId(), release->getId()));

        // Report the first GENRE for this track
        const ClusterType::pointer genreClusterType{ ClusterType::find(context.getDbSession(), "GENRE") };
        if (genreClusterType)
        {
            const auto clusters{ release->getClusters(genreClusterType->getId(), 1) };
            if (!clusters.empty())
                albumNode.setAttribute("genre", clusters.front()->getName());
        }

        if (const Wt::WDateTime dateTime{ core::Service<feedback::IFeedbackService>::get()->getStarredDateTime(context.getUser()->getId(), release->getId()) }; dateTime.isValid())
            albumNode.setAttribute("starred", core::stringUtils::toISO8601String(dateTime));

        // Always report user rating, even if legacy API only specified it for directories
        if (const auto rating{ core::Service<feedback::IFeedbackService>::get()->getRating(context.getUser()->getId(), release->getId()) })
            albumNode.setAttribute("userRating", *rating);

        if (!context.isOpenSubsonicEnabled())
            return albumNode;

        // OpenSubsonic specific fields (must always be set)
        albumNode.setAttribute("version", release->getComment());
        albumNode.setAttribute("sortName", release->getSortName());
        albumNode.setAttribute("mediaType", "album");

        {
            const Wt::WDateTime dateTime{ core::Service<scrobbling::IScrobblingService>::get()->getLastListenDateTime(context.getUser()->getId(), release->getId()) };
            albumNode.setAttribute("played", dateTime.isValid() ? core::stringUtils::toISO8601String(dateTime) : std::string{ "" });
        }

        {
            std::optional<core::UUID> mbid{ release->getMBID() };
            albumNode.setAttribute("musicBrainzId", mbid ? mbid->getAsString() : "");
        }

        auto addClusters{ [&](Response::Node::Key field, std::string_view clusterTypeName) {
            albumNode.createEmptyArrayValue(field);

            Cluster::FindParameters params;
            params.setRelease(release->getId());
            params.setClusterTypeName(clusterTypeName);

            Cluster::find(context.getDbSession(), params, [&](const Cluster::pointer& cluster) {
                albumNode.addArrayValue(field, cluster->getName());
            });
        } };

        addClusters("moods", "MOOD");

        // Genres
        albumNode.createEmptyArrayChild("genres");
        if (genreClusterType)
        {
            Cluster::FindParameters params;
            params.setRelease(release->getId());
            params.setClusterType(genreClusterType->getId());

            Cluster::find(context.getDbSession(), params, [&](const Cluster::pointer& cluster) {
                albumNode.addArrayChild("genres", createItemGenreNode(cluster->getName()));
            });
        }

        if (id3)
        {
            albumNode.createEmptyArrayChild("artists");
            for (const Artist::pointer& artist : release->getReleaseArtists())
                albumNode.addArrayChild("artists", createArtistNode(artist));

            albumNode.setAttribute("displayArtist", release->getArtistDisplayName());
        }
        else
        {
            albumNode.createEmptyArrayChild("albumArtists");
            for (const Artist::pointer& artist : release->getReleaseArtists())
                albumNode.addArrayChild("albumArtists", createArtistNode(artist));

            albumNode.setAttribute("displayAlbumArtist", release->getArtistDisplayName());

            albumNode.createEmptyArrayChild("artists");
            albumNode.setAttribute("displayArtist", "");
        }
        albumNode.addChild("originalReleaseDate", createItemDateNode(release->getOriginalDate()));
        albumNode.addChild("releaseDate", createItemDateNode(release->getDate()));

        albumNode.setAttribute("isCompilation", release->isCompilation());

        albumNode.createEmptyArrayValue("releaseTypes");
        for (std::string_view releaseType : release->getReleaseTypeNames())
            albumNode.addArrayValue("releaseTypes", releaseType);

        albumNode.createEmptyArrayChild("discTitles");
        for (const auto& medium : release->getMediums())
        {
            if (medium->getName().empty())
                continue;

            albumNode.addArrayChild("discTitles", createDiscTitle(medium));
        }

        albumNode.createEmptyArrayChild("recordLabels");
        release->visitLabels([&](const Label::pointer& label) {
            albumNode.addArrayChild("recordLabels", createRecordLabel(label));
        });

        auto advisoryToExplicitStatus = [&](const core::EnumSet<db::Advisory> advisories) -> std::string_view {
            if (advisories.contains(db::Advisory::Explicit))
                return "explicit";

            if (advisories.contains(db::Advisory::Clean))
                return "clean";

            return "";
        };

        albumNode.setAttribute("explicitStatus", advisoryToExplicitStatus(release->getAdvisories()));

        return albumNode;
    }
} // namespace lms::api::subsonic