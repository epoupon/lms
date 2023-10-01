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

#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Release.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"
#include "responses/Artist.hpp"
#include "SubsonicId.hpp"
#include "Utils.hpp"

namespace API::Subsonic
{
    using namespace Database;

    static const std::string_view reportedDummyStarredDate{ "2000-01-01T00:00:00" };

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

    Response::Node createSongNode(const Track::pointer& track, Session& dbSession, const User::pointer& user)
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

        trackResponse.setAttribute("path", getTrackPath(track));
        {
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

        if (user->getSubsonicTranscodeEnable())
            trackResponse.setAttribute("transcodedSuffix", formatToSuffix(user->getSubsonicTranscodeFormat()));

        trackResponse.setAttribute("coverArt", idToString(track->getId()));

        const std::vector<Artist::pointer>& artists{ track->getArtists({TrackArtistLinkType::Artist}) };
        if (!artists.empty())
        {
            trackResponse.setAttribute("artist", Utils::joinArtistNames(artists));

            if (artists.size() == 1)
                trackResponse.setAttribute("artistId", idToString(artists.front()->getId()));
        }

        if (track->getRelease())
        {
            trackResponse.setAttribute("album", track->getRelease()->getName());
            trackResponse.setAttribute("albumId", idToString(track->getRelease()->getId()));
            trackResponse.setAttribute("parent", idToString(track->getRelease()->getId()));
        }

        trackResponse.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count());
        trackResponse.setAttribute("type", "music");
        trackResponse.setAttribute("created", StringUtils::toISO8601String(track->getLastWritten()));

        if (Service<Scrobbling::IScrobblingService>::get()->isStarred(user->getId(), track->getId()))
            trackResponse.setAttribute("starred", reportedDummyStarredDate); // TODO handle date/time

        // Report the first GENRE for this track
        ClusterType::pointer clusterType{ ClusterType::find(dbSession, "GENRE") };
        if (clusterType)
        {
            auto clusters{ track->getClusterGroups({clusterType}, 1) };
            if (!clusters.empty() && !clusters.front().empty())
                trackResponse.setAttribute("genre", clusters.front().front()->getName());
        }

        return trackResponse;
    }
}