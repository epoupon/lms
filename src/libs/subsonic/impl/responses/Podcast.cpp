/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "Podcast.hpp"

#include <chrono>

#include <Wt/WDate.h>

#include "core/String.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"

#include "CoverArtId.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    std::string_view getStatus(const db::PodcastEpisode::pointer& episode)
    {
        if (episode->getManualDownloadState() == db::PodcastEpisode::ManualDownloadState::DeleteRequested)
            return "deleted";

        if (!episode->getAudioRelativeFilePath().empty())
            return "completed";

        return "new";
    }

    Response::Node createPodcastEpisodeNode(const db::PodcastEpisode::pointer& episode)
    {
        Response::Node episodeNode;

        // Child attributes
        episodeNode.setAttribute("id", idToString(episode->getId()));
        episodeNode.setAttribute("title", episode->getTitle());
        if (episode->getPubDate().isValid())
            episodeNode.setAttribute("year", std::to_string(episode->getPubDate().date().year()));
        if (!episode->getEnclosureContentType().empty())
            episodeNode.setAttribute("contentType", episode->getEnclosureContentType());
        episodeNode.setAttribute("duration", std::chrono::duration_cast<std::chrono::seconds>(episode->getDuration()).count());
        if (episode->getEnclosureLength() > 0)
            episodeNode.setAttribute("size", episode->getEnclosureLength());
        episodeNode.setAttribute("isDir", "false"); // TODO parent?
        if (!episode->getEnclosureUrl().empty())
        {
            const auto pos{ episode->getEnclosureUrl().find_last_of('.') };
            if (pos != std::string_view::npos)
                episodeNode.setAttribute("suffix", episode->getEnclosureUrl().substr(pos + 1));
        }
        // estimated bitrate
        if (episode->getEnclosureLength() > 0 && episode->getDuration() > std::chrono::milliseconds::zero())
            episodeNode.setAttribute("bitrate", episode->getEnclosureLength() * 8 / std::chrono::duration_cast<std::chrono::milliseconds>(episode->getDuration()).count());
        if (const auto artwork{ episode->getArtwork() })
        {
            CoverArtId coverArtId{ artwork->getId(), artwork->getLastWrittenTime().toTime_t() };
            episodeNode.setAttribute("coverArt", idToString(coverArtId));
        }

        // Podcast specific attributes
        // Expose the streamId only if the episode is actually downloaded
        if (!episode->getAudioRelativeFilePath().empty())
            episodeNode.setAttribute("streamId", idToString(episode->getId())); // Use this ID for streaming the podcast
        episodeNode.setAttribute("channelId", idToString(episode->getPodcastId()));
        episodeNode.setAttribute("description", episode->getDescription());
        episodeNode.setAttribute("status", getStatus(episode));
        if (episode->getPubDate().isValid())
            episodeNode.setAttribute("publishDate", core::stringUtils::toISO8601String(episode->getPubDate()));

        return episodeNode;
    }

    std::string_view getStatus(const db::Podcast::pointer& podcast)
    {
        if (podcast->getTitle().empty())
            return "new";

        return "completed";
    }

    Response::Node createPodcastNode(RequestContext& context, const db::Podcast::pointer& podcast, bool includeEpisodes)
    {
        Response::Node podcastNode;

        podcastNode.setAttribute("id", idToString(podcast->getId()));
        podcastNode.setAttribute("url", podcast->getLink()); // TODO
        if (!podcast->getTitle().empty())
            podcastNode.setAttribute("title", podcast->getTitle());
        if (!podcast->getDescription().empty())
            podcastNode.setAttribute("description", podcast->getDescription());
        if (!podcast->getImageUrl().empty())
            podcastNode.setAttribute("originalImageUrl", podcast->getImageUrl());

        podcastNode.setAttribute("status", getStatus(podcast));

        if (const auto artwork{ podcast->getArtwork() })
        {
            CoverArtId coverArtId{ artwork->getId(), artwork->getLastWrittenTime().toTime_t() };
            podcastNode.setAttribute("coverArt", idToString(coverArtId));
        }

        if (includeEpisodes)
        {
            podcastNode.createEmptyArrayChild("episode ");

            db::PodcastEpisode::FindParameters params;
            params.setPodcast(podcast->getId());
            params.setSortMode(db::PodcastEpisodeSortMode::PubDateDesc);

            db::PodcastEpisode::find(context.dbSession, params, [&](const db::PodcastEpisode::pointer& episode) {
                podcastNode.addArrayChild("episode", createPodcastEpisodeNode(episode));
            });
        }

        return podcastNode;
    }
} // namespace lms::api::subsonic