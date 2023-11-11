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

#include "responses/Song.hpp"

#include <string_view>

#include "av/IAudioFile.hpp"
#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Listen.hpp"
#include "services/database/Release.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "services/database/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"
#include "responses/Artist.hpp"
#include "responses/Contributor.hpp"
#include "responses/ItemGenre.hpp"
#include "responses/ReplayGain.hpp"
#include "SubsonicId.hpp"
#include "Utils.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace
    {
        std::string_view formatToSuffix(AudioFormat format)
        {
            switch (format)
            {
            case AudioFormat::MP3:              return "mp3";
            case AudioFormat::OGG_OPUS:         return "opus";
            case AudioFormat::MATROSKA_OPUS:    return "mka";
            case AudioFormat::OGG_VORBIS:       return "ogg";
            case AudioFormat::WEBM_VORBIS:      return "webm";
            }

            return "";
        }

        std::string getTrackPath(const Track::pointer& track)
        {
            std::string path;

            // The track path has to be relative from the root

            const auto release{ track->getRelease() };
            if (release)
            {
                auto artists{ release->getReleaseArtists() };
                if (artists.empty())
                    artists = release->getArtists();

                if (artists.size() > 1)
                    path = "Various Artists/";
                else if (artists.size() == 1)
                    path = Utils::makeNameFilesystemCompatible(artists.front()->getName()) + "/";

                path += Utils::makeNameFilesystemCompatible(track->getRelease()->getName()) + "/";
            }

            if (track->getDiscNumber())
                path += std::to_string(*track->getDiscNumber()) + "-";
            if (track->getTrackNumber())
                path += std::to_string(*track->getTrackNumber()) + "-";

            path += Utils::makeNameFilesystemCompatible(track->getName());

            if (track->getPath().has_extension())
                path += track->getPath().extension();

            return path;
        }
    }

    Response::Node createSongNode(RequestContext& context, const Track::pointer& track, const User::pointer& user)
    {
        Response::Node trackResponse;

        trackResponse.setAttribute("id", idToString(track->getId()));
        trackResponse.setAttribute("isDir", false);
        trackResponse.setAttribute("title", track->getName());
        if (track->getTrackNumber())
            trackResponse.setAttribute("track", *track->getTrackNumber());
        if (track->getDiscNumber())
            trackResponse.setAttribute("discNumber", *track->getDiscNumber());
        if (track->getYear())
            trackResponse.setAttribute("year", *track->getYear());
        trackResponse.setAttribute("playCount", Listen::getCount(context.dbSession, user->getId(), user->getScrobblingBackend(), track->getId()));
        trackResponse.setAttribute("path", getTrackPath(track));
        {
            // TODO, store this in DB
            std::error_code ec;
            const auto fileSize{ std::filesystem::file_size(track->getPath(), ec) };
            if (!ec)
                trackResponse.setAttribute("size", fileSize);
        }

        if (track->getPath().has_extension())
        {
            auto extension{ track->getPath().extension() };
            trackResponse.setAttribute("suffix", extension.string().substr(1));
        }

        {
            const std::string fileSuffix{ formatToSuffix(user->getSubsonicDefaultTranscodeFormat()) };
            trackResponse.setAttribute("transcodedSuffix", fileSuffix);
            trackResponse.setAttribute("transcodedContentType", Av::getMimeType(std::filesystem::path{ "." + fileSuffix }));
        }

        trackResponse.setAttribute("coverArt", idToString(track->getId()));

        const std::vector<Artist::pointer>& artists{ track->getArtists({TrackArtistLinkType::Artist}) };
        if (!artists.empty())
        {
            if (!track->getArtistDisplayName().empty())
                trackResponse.setAttribute("artist", track->getArtistDisplayName());
            else
                trackResponse.setAttribute("artist", Utils::joinArtistNames(artists));

            if (artists.size() == 1)
                trackResponse.setAttribute("artistId", idToString(artists.front()->getId()));
        }

        Release::pointer release{ track->getRelease() };
        if (release)
        {
            trackResponse.setAttribute("album", release->getName());
            trackResponse.setAttribute("albumId", idToString(release->getId()));
            trackResponse.setAttribute("parent", idToString(release->getId()));
        }

        trackResponse.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count());
        trackResponse.setAttribute("bitRate", (track->getBitrate() / 1000));
        trackResponse.setAttribute("type", "music");
        trackResponse.setAttribute("created", StringUtils::toISO8601String(track->getLastWritten()));
        trackResponse.setAttribute("contentType", Av::getMimeType(track->getPath().extension()));

        if (const Wt::WDateTime dateTime{ Service<Feedback::IFeedbackService>::get()->getStarredDateTime(user->getId(), track->getId()) }; dateTime.isValid())
            trackResponse.setAttribute("starred", StringUtils::toISO8601String(dateTime));

        // Report the first GENRE for this track
        const ClusterType::pointer genreClusterType{ ClusterType::find(context.dbSession, "GENRE") };
        if (genreClusterType)
        {
            auto clusters{ track->getClusterGroups({genreClusterType}, 1) };
            if (!clusters.empty() && !clusters.front().empty())
                trackResponse.setAttribute("genre", clusters.front().front()->getName());
        }

        // OpenSubsonic specific fields (must always be set)
        if (!context.enableOpenSubsonic)
            return trackResponse;

        trackResponse.setAttribute("mediaType", "song");

        {
            const Wt::WDateTime dateTime{ Service<Scrobbling::IScrobblingService>::get()->getLastListenDateTime(user->getId(), track->getId()) };
            trackResponse.setAttribute("played", dateTime.isValid() ? StringUtils::toISO8601String(dateTime) : "");
        }

        {
            std::optional<UUID> mbid{ track->getRecordingMBID() };
            trackResponse.setAttribute("musicBrainzId", mbid ? mbid->getAsString() : "");
        }

        trackResponse.createEmptyArrayChild("contributors");
        {
            TrackArtistLink::FindParameters params;
            params.setTrack(track->getId());

            for (const TrackArtistLinkId linkId : TrackArtistLink::find(context.dbSession, params).results)
            {
                TrackArtistLink::pointer link{ TrackArtistLink::find(context.dbSession, linkId) };
                // Don't report artists nor release artists as they are set in dedicated fields
                if (link && link->getType() != TrackArtistLinkType::Artist && link->getType() != TrackArtistLinkType::ReleaseArtist)
                    trackResponse.addArrayChild("contributors", createContributorNode(link));
            }
        }

        auto addArtistLinks{ [&](Response::Node::Key nodeName, TrackArtistLinkType type)
        {
            trackResponse.createEmptyArrayChild(nodeName);

            TrackArtistLink::FindParameters params;
            params.setTrack(track->getId());
            params.setLinkType(type);

            for (const TrackArtistLinkId linkId : TrackArtistLink::find(context.dbSession, params).results)
            {
                TrackArtistLink::pointer link{ TrackArtistLink::find(context.dbSession, linkId) };
                if (link)
                    trackResponse.addArrayChild(nodeName, createArtistNode(link->getArtist()));
            }
        } };

        addArtistLinks("artists", TrackArtistLinkType::Artist);
        trackResponse.setAttribute("displayArtist", track->getArtistDisplayName());

        addArtistLinks("albumartists", TrackArtistLinkType::ReleaseArtist);
        if (release)
            trackResponse.setAttribute("displayAlbumArtist", release->getArtistDisplayName());

        auto addClusters{ [&](Response::Node::Key field, std::string_view clusterTypeName)
        {
            trackResponse.createEmptyArrayValue(field);

            ClusterType::pointer clusterType{ ClusterType::find(context.dbSession, clusterTypeName) };
            if (clusterType)
            {
                Cluster::FindParameters params;
                params.setTrack(track->getId());
                params.setClusterType(clusterType->getId());

                for (const auto& cluster : Cluster::find(context.dbSession, params).results)
                    trackResponse.addArrayValue(field, cluster->getName());
            }
        } };

        addClusters("moods", "MOOD");

        // Genres
        trackResponse.createEmptyArrayChild("genres");
        if (genreClusterType)
        {
            Cluster::FindParameters params;
            params.setTrack(track->getId());
            params.setClusterType(genreClusterType->getId());

            for (const auto& cluster : Cluster::find(context.dbSession, params).results)
                trackResponse.addArrayChild("genres", createItemGenreNode(cluster->getName()));
        }

        trackResponse.addChild("replayGain", createReplayGainNode(track));

        return trackResponse;
    }
}