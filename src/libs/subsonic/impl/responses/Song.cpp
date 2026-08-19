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

#include <filesystem>
#include <string_view>
#include <system_error>

#include "core/ITraceLogger.hpp"
#include "core/MimeTypes.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"

#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/User.hpp"
#include "database/objects/Work.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "responses/Artist.hpp"
#include "responses/Contributor.hpp"
#include "responses/ItemGenre.hpp"
#include "responses/ReplayGain.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        std::string_view formatToSuffix(db::TranscodingOutputFormat format)
        {
            switch (format)
            {
            case db::TranscodingOutputFormat::MP3:
                return "mp3";
            case db::TranscodingOutputFormat::OGG_OPUS:
                return "opus";
            case db::TranscodingOutputFormat::OGG_VORBIS:
                return "ogg";
            }

            return "";
        }
    } // namespace

    Response::Node createSongNode(RequestContext& context, const db::Track::pointer& track, bool id3)
    {
        LMS_SCOPED_TRACE_DETAILED("Subsonic", "CreateSong");

        const auto medium{ track->getMedium() };

        Response::Node trackResponse;

        if (!id3)
        {
            if (const auto directory{ track->getDirectory() })
                trackResponse.setAttribute("parent", idToString(directory->getId()));
        }

        trackResponse.setAttribute("isDir", false);
        trackResponse.setAttribute("id", idToString(track->getId()));
        trackResponse.setAttribute("title", track->getName());
        if (track->getTrackNumber())
            trackResponse.setAttribute("track", *track->getTrackNumber());
        if (medium && medium->getPosition())
            trackResponse.setAttribute("discNumber", *medium->getPosition());
        if (const auto originalYear{ track->getOriginalYear() })
            trackResponse.setAttribute("year", *originalYear);
        else if (const auto year{ track->getYear() })
            trackResponse.setAttribute("year", *year);
        trackResponse.setAttribute("playCount", core::Service<scrobbling::IScrobblingService>::get()->getCount(context.getUser()->getId(), track->getId()));

        // maybe not available if user just removed the library without rescanning
        if (const db::MediaLibrary::pointer library{ track->getMediaLibrary() })
        {
            std::error_code ec;
            const std::filesystem::path relativeTrackPath{ std::filesystem::relative(track->getAbsoluteFilePath(), library->getPath(), ec) };
            if (!ec && !relativeTrackPath.empty())
                trackResponse.setAttribute("path", relativeTrackPath.c_str());
        }

        trackResponse.setAttribute("size", track->getFileSize());

        if (track->getAbsoluteFilePath().has_extension())
        {
            auto extension{ track->getAbsoluteFilePath().extension() };
            trackResponse.setAttribute("suffix", extension.string().substr(1) /* skip leading .*/);
        }

        if (context.getUser()->getSubsonicEnableTranscodingByDefault())
        {
            const std::string fileSuffix{ formatToSuffix(context.getUser()->getSubsonicDefaultTranscodingOutputFormat()) };
            trackResponse.setAttribute("transcodedSuffix", fileSuffix);
            trackResponse.setAttribute("transcodedContentType", core::getMimeType(std::filesystem::path{ "." + fileSuffix }));
        }

        const db::ArtworkId artworkId{ track->getPreferredMediaArtworkId().isValid() ? track->getPreferredMediaArtworkId() : track->getPreferredArtworkId() };
        if (artworkId.isValid())
            trackResponse.setAttribute("coverArt", idToString(artworkId));

        std::vector<db::TrackArtistLink::pointer> artistLinks;
        std::vector<db::TrackArtistLink::pointer> trackArtistLinks;
        track->visitArtistLinks([&](const db::TrackArtistLink::pointer& link) {
            artistLinks.push_back(link);

            if (link->getType() == db::TrackArtistLinkType::Artist)
                trackArtistLinks.push_back(link);
        });

        if (!trackArtistLinks.empty())
        {
            if (!track->getArtistDisplayName().empty())
                trackResponse.setAttribute("artist", track->getArtistDisplayName());
            else
                trackResponse.setAttribute("artist", utils::joinArtistNames(trackArtistLinks));

            if (trackArtistLinks.size() == 1)
                trackResponse.setAttribute("artistId", idToString(trackArtistLinks.front()->getArtistId()));
        }

        const db::Release::pointer release{ track->getRelease() };
        if (release)
        {
            trackResponse.setAttribute("album", release->getName());
            trackResponse.setAttribute("albumId", idToString(release->getId()));
        }

        trackResponse.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count());
        trackResponse.setAttribute("bitRate", (track->getBitrate() / 1000));
        trackResponse.setAttribute("type", "music");
        trackResponse.setAttribute("created", core::stringUtils::toISO8601String(track->getAddedTime()));
        trackResponse.setAttribute("contentType", core::getMimeType(track->getAbsoluteFilePath().extension()));
        if (const auto rating{ core::Service<feedback::IFeedbackService>::get()->getRating(context.getUser()->getId(), track->getId()) })
            trackResponse.setAttribute("userRating", *rating);

        if (const Wt::WDateTime dateTime{ core::Service<feedback::IFeedbackService>::get()->getStarredDateTime(context.getUser()->getId(), track->getId()) }; dateTime.isValid())
            trackResponse.setAttribute("starred", core::stringUtils::toISO8601String(dateTime));

        // Report the first genre for this track
        const auto genres{ track->getGenres() };
        if (!genres.empty())
            trackResponse.setAttribute("genre", genres.front()->getName());

        // OpenSubsonic specific fields (must always be set)
        if (!context.isOpenSubsonicEnabled())
            return trackResponse;

        trackResponse.setAttribute("comment", track->getComment());
        trackResponse.setAttribute("bitDepth", track->getBitsPerSample() ? *track->getBitsPerSample() : 0);
        trackResponse.setAttribute("samplingRate", track->getSampleRate());
        trackResponse.setAttribute("channelCount", track->getChannelCount());

        trackResponse.setAttribute("mediaType", "song");

        {
            const Wt::WDateTime dateTime{ core::Service<scrobbling::IScrobblingService>::get()->getLastListenDateTime(context.getUser()->getId(), track->getId()) };
            trackResponse.setAttribute("played", dateTime.isValid() ? core::stringUtils::toISO8601String(dateTime) : "");
        }

        {
            std::optional<core::UUID> mbid{ track->getRecordingMBID() };
            trackResponse.setAttribute("musicBrainzId", mbid ? mbid->toString() : "");
        }

        {
            trackResponse.createEmptyArrayChild("albumArtists");
            trackResponse.createEmptyArrayChild("artists");
            trackResponse.createEmptyArrayChild("contributors");

            for (const auto& artistLink : artistLinks)
            {
                switch (artistLink->getType())
                {
                case db::TrackArtistLinkType::Artist:
                    trackResponse.addArrayChild("artists", createMinimalArtistNode(artistLink));
                    break;
                default:
                    trackResponse.addArrayChild("contributors", createContributorNode(artistLink));
                }
            }

            if (release)
            {
                release->visitArtistLinks([&](const db::ReleaseArtistLink::pointer& artistLink) {
                    trackResponse.addArrayChild("albumArtists", createMinimalArtistNode(artistLink));
                });
            }
        }

        trackResponse.setAttribute("displayArtist", track->getArtistDisplayName());
        if (release)
            trackResponse.setAttribute("displayAlbumArtist", release->getArtistDisplayName());

        trackResponse.createEmptyArrayValue("moods");
        for (const auto& mood : track->getMoods())
            trackResponse.addArrayValue("moods", mood->getName());

        trackResponse.createEmptyArrayValue("groupings");
        for (const auto& grouping : track->getGroupings())
            trackResponse.addArrayValue("groupings", grouping->getName());

        // Genres
        trackResponse.createEmptyArrayChild("genres");
        for (const auto& genre : genres)
            trackResponse.addArrayChild("genres", createItemGenreNode(genre->getName()));

        auto advisoryToExplicitStatus = [](db::Advisory advisory) -> std::string_view {
            switch (advisory)
            {
            case db::Advisory::Clean:
                return "clean";
            case db::Advisory::Explicit:
                return "explicit";
            case db::Advisory::Unknown:
            case db::Advisory::UnSet:
                break;
            }

            return "";
        };
        trackResponse.setAttribute("explicitStatus", advisoryToExplicitStatus(track->getAdvisory()));

        trackResponse.addChild("replayGain", createReplayGainNode(track, medium));

        trackResponse.createEmptyArrayChild("works");
        for (const auto& work : track->getWorks())
        {
            Response::Node workNode;
            workNode.setAttribute("name", work->getName());
            if (const auto mbid{ work->getMBID() })
                workNode.setAttribute("musicBrainzId", mbid->toString());
            trackResponse.addArrayChild("works", std::move(workNode));
        }

        trackResponse.createEmptyArrayChild("movements");
        for (const auto& movement : track->getMovements())
        {
            Response::Node movementNode;
            movementNode.setAttribute("name", movement->getName());
            if (const auto n{ movement->getNumber() })
                movementNode.setAttribute("number", *n);
            if (const auto c{ movement->getCount() })
                movementNode.setAttribute("count", *c);
            trackResponse.addArrayChild("movements", std::move(movementNode));
        }

        return trackResponse;
    }
} // namespace lms::api::subsonic