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

#include "database/Session.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"
#include "services/podcast/IPodcastService.hpp"

#include "ParameterParsing.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"
#include "responses/Podcast.hpp"

namespace lms::api::subsonic
{
    Response handleGetPodcasts(RequestContext& context)
    {
        const bool includeEpisodes{ getParameterAs<bool>(context.getParameters(), "includeEpisodes").value_or(true) };
        const std::optional<db::PodcastId> podcastId{ getParameterAs<db::PodcastId>(context.getParameters(), "id") };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node& podcastsNode{ response.createNode("podcasts") };
        podcastsNode.createEmptyArrayChild("channel");

        auto transaction{ context.getDbSession().createReadTransaction() };

        auto processPodcast{ [&](const db::Podcast::pointer& podcast) {
            podcastsNode.addArrayChild("channel", createPodcastNode(context, podcast, includeEpisodes));
        } };

        if (podcastId.has_value())
        {
            db::Podcast::pointer podcast{ db::Podcast::find(context.getDbSession(), podcastId.value()) };
            if (!podcast)
                throw RequestedDataNotFoundError{};

            processPodcast(podcast);
        }
        else
            db::Podcast::find(context.getDbSession(), processPodcast);

        return response;
    }

    Response handleGetNewestPodcasts(RequestContext& context)
    {
        std::size_t count{ getParameterAs<std::size_t>(context.getParameters(), "count").value_or(20) };
        count = std::min<std::size_t>(count, 100);

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };
        Response::Node& newestPodcastsNode{ response.createNode("newestPodcasts") };
        newestPodcastsNode.createEmptyArrayChild("episode");

        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            db::PodcastEpisode::FindParameters findParameters;
            findParameters.setRange(db::Range{ .offset = 0, .size = count });

            db::PodcastEpisode::find(context.getDbSession(), findParameters, [&](const db::PodcastEpisode::pointer& episode) {
                newestPodcastsNode.addArrayChild("episode", createPodcastEpisodeNode(episode));
            });
        }

        return response;
    }

    Response handleRefreshPodcasts(RequestContext& context)
    {
        core::Service<podcast::IPodcastService>::get()->refreshPodcasts();

        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleCreatePodcastChannel(RequestContext& context)
    {
        // Mandatory parameters
        const std::string url{ getMandatoryParameterAs<std::string>(context.getParameters(), "url") };

        if (url.empty() || !(url.starts_with("http://") || url.starts_with("https://")))
            throw BadParameterGenericError{ "url", "must start by http:// or https://" };

        // no effect if podcast already exists
        core::Service<podcast::IPodcastService>::get()->addPodcast(url);

        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleDeletePodcastChannel(RequestContext& context)
    {
        // Mandatory parameters
        const db::PodcastId podcastId{ getMandatoryParameterAs<db::PodcastId>(context.getParameters(), "id") };

        if (!core::Service<podcast::IPodcastService>::get()->removePodcast(podcastId))
            throw RequestedDataNotFoundError{};

        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleDeletePodcastEpisode(RequestContext& context)
    {
        // Mandatory parameters
        const db::PodcastEpisodeId episodeId{ getMandatoryParameterAs<db::PodcastEpisodeId>(context.getParameters(), "id") };

        if (!core::Service<podcast::IPodcastService>::get()->deletePodcastEpisode(episodeId))
            throw RequestedDataNotFoundError{};

        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleDownloadPodcastEpisode(RequestContext& context)
    {
        // Mandatory parameters
        const db::PodcastEpisodeId episodeId{ getMandatoryParameterAs<db::PodcastEpisodeId>(context.getParameters(), "id") };

        if (!core::Service<podcast::IPodcastService>::get()->downloadPodcastEpisode(episodeId))
            throw RequestedDataNotFoundError{};

        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleGetPodcastEpisode(RequestContext& context)
    {
        // Mandatory parameters
        const db::PodcastEpisodeId episodeId{ getMandatoryParameterAs<db::PodcastEpisodeId>(context.getParameters(), "id") };

        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };

        auto transaction{ context.getDbSession().createReadTransaction() };

        const db::PodcastEpisode::pointer episode{ db::PodcastEpisode::find(context.getDbSession(), episodeId) };
        if (!episode)
            throw RequestedDataNotFoundError{};

        response.addNode("podcastEpisode", createPodcastEpisodeNode(episode));

        return response;
    }
} // namespace lms::api::subsonic
