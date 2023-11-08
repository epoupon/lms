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

#include "AlbumSongLists.hpp"

#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "responses/Album.hpp"
#include "responses/Artist.hpp"
#include "responses/Song.hpp"
#include "utils/Service.hpp"
#include "ParameterParsing.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace
    {
        Response handleGetAlbumListRequestCommon(RequestContext& context, bool id3)
        {
            // Mandatory params
            const std::string type{ getMandatoryParameterAs<std::string>(context.parameters, "type") };

            // Optional params
            const std::size_t size{ getParameterAs<std::size_t>(context.parameters, "size").value_or(10) };
            const std::size_t offset{ getParameterAs<std::size_t>(context.parameters, "offset").value_or(0) };
            if (size > 500)
                throw ParameterValueTooHighGenericError{ "size", 500 };

            const Range range{ offset, size };

            RangeResults<ReleaseId> releases;
            Scrobbling::IScrobblingService& scrobblingService{ *Service<Scrobbling::IScrobblingService>::get() };
            Feedback::IFeedbackService& feedbackService{ *Service<Feedback::IFeedbackService>::get() };

            auto transaction{ context.dbSession.createSharedTransaction() };

            User::pointer user{ User::find(context.dbSession, context.userId) };
            if (!user)
                throw UserNotAuthorizedError{};

            if (type == "alphabeticalByName")
            {
                Release::FindParameters params;
                params.setSortMethod(ReleaseSortMethod::Name);
                params.setRange(range);

                releases = Release::findIds(context.dbSession, params);
            }
            else if (type == "alphabeticalByArtist")
            {
                releases = Release::findIdsOrderedByArtist(context.dbSession, range);
            }
            else if (type == "byGenre")
            {
                // Mandatory param
                const std::string genre{ getMandatoryParameterAs<std::string>(context.parameters, "genre") };

                if (const ClusterType::pointer clusterType{ ClusterType::find(context.dbSession, "GENRE") })
                {
                    if (const Cluster::pointer cluster{ clusterType->getCluster(genre) })
                    {
                        Release::FindParameters params;
                        params.setClusters({ cluster->getId() });
                        params.setSortMethod(ReleaseSortMethod::Name);
                        params.setRange(range);

                        releases = Release::findIds(context.dbSession, params);
                    }
                }
            }
            else if (type == "byYear")
            {
                const int fromYear{ getMandatoryParameterAs<int>(context.parameters, "fromYear") };
                const int toYear{ getMandatoryParameterAs<int>(context.parameters, "toYear") };

                Release::FindParameters params;
                params.setSortMethod(ReleaseSortMethod::Date);
                params.setRange(range);
                params.setDateRange(DateRange::fromYearRange(fromYear, toYear));

                releases = Release::findIds(context.dbSession, params);
            }
            else if (type == "frequent")
            {
                releases = scrobblingService.getTopReleases(context.userId, {}, range);
            }
            else if (type == "newest")
            {
                Release::FindParameters params;
                params.setSortMethod(ReleaseSortMethod::LastWritten);
                params.setRange(range);

                releases = Release::findIds(context.dbSession, params);
            }
            else if (type == "random")
            {
                // Random results are paginated, but there is no acceptable way to handle the pagination params without repeating some albums
                // (no seed provided by subsonic, ot it would require to store some kind of context for each user/client when iterating over the random albums)
                Release::FindParameters params;
                params.setSortMethod(ReleaseSortMethod::Random);
                params.setRange({ 0, size });

                releases = Release::findIds(context.dbSession, params);
            }
            else if (type == "recent")
            {
                releases = scrobblingService.getRecentReleases(context.userId, {}, range);
            }
            else if (type == "starred")
            {
                releases = feedbackService.getStarredReleases(context.userId, {}, range);
            }
            else
                throw NotImplementedGenericError{};

            Response response{ Response::createOkResponse(context.serverProtocolVersion) };
            Response::Node& albumListNode{ response.createNode(id3 ? Response::Node::Key{ "albumList2" } : Response::Node::Key{ "albumList" }) };

            for (const ReleaseId releaseId : releases.results)
            {
                const Release::pointer release{ Release::find(context.dbSession, releaseId) };
                albumListNode.addArrayChild("album", createAlbumNode(context, release, user, id3));
            }

            return response;
        }

        Response handleGetStarredRequestCommon(RequestContext& context, bool id3)
        {
            auto transaction{ context.dbSession.createSharedTransaction() };

            User::pointer user{ User::find(context.dbSession, context.userId) };
            if (!user)
                throw UserNotAuthorizedError{};

            Response response{ Response::createOkResponse(context.serverProtocolVersion) };
            Response::Node& starredNode{ response.createNode(id3 ? Response::Node::Key{ "starred2" } : Response::Node::Key{ "starred" }) };

            Feedback::IFeedbackService& feedbackService{ *Service<Feedback::IFeedbackService>::get() };

            for (const ArtistId artistId : feedbackService.getStarredArtists(context.userId, {} /* clusters */, std::nullopt /* linkType */, ArtistSortMethod::BySortName, Range{}).results)
            {
                if (auto artist{ Artist::find(context.dbSession, artistId) })
                    starredNode.addArrayChild("artist", createArtistNode(context, artist, user, id3));
            }

            for (const ReleaseId releaseId : feedbackService.getStarredReleases(context.userId, {} /* clusters */, Range{}).results)
            {
                if (auto release{ Release::find(context.dbSession, releaseId) })
                    starredNode.addArrayChild("album", createAlbumNode(context, release, user, id3));
            }

            for (const TrackId trackId : feedbackService.getStarredTracks(context.userId, {} /* clusters */, Range{}).results)
            {
                if (auto track{ Track::find(context.dbSession, trackId) })
                    starredNode.addArrayChild("song", createSongNode(context, track, user));
            }

            return response;
        }
    } // namespace

    Response handleGetAlbumListRequest(RequestContext& context)
    {
        return handleGetAlbumListRequestCommon(context, false /* no id3 */);
    }

    Response handleGetAlbumList2Request(RequestContext& context)
    {
        return handleGetAlbumListRequestCommon(context, true /* id3 */);
    }

    Response handleGetRandomSongsRequest(RequestContext& context)
    {
        // Optional params
        std::size_t size{ getParameterAs<std::size_t>(context.parameters, "size").value_or(50) };
        if (size > 500)
            throw ParameterValueTooHighGenericError{"size", 500};

        auto transaction{ context.dbSession.createSharedTransaction() };

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        const auto tracks{ Track::find(context.dbSession, Track::FindParameters {}.setSortMethod(TrackSortMethod::Random).setRange({0, size})) };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };

        Response::Node& randomSongsNode{ response.createNode("randomSongs") };
        for (const Track::pointer& track : tracks.results)
            randomSongsNode.addArrayChild("song", createSongNode(context, track, user));

        return response;
    }

    Response handleGetSongsByGenreRequest(RequestContext& context)
    {
        // Mandatory params
        std::string genre{ getMandatoryParameterAs<std::string>(context.parameters, "genre") };

        // Optional params
        std::size_t count{ getParameterAs<std::size_t>(context.parameters, "count").value_or(10) };
        if (count > 500)
            throw ParameterValueTooHighGenericError{"count", 500};

        std::size_t offset{ getParameterAs<std::size_t>(context.parameters, "offset").value_or(0) };

        auto transaction{ context.dbSession.createSharedTransaction() };

        auto clusterType{ ClusterType::find(context.dbSession, "GENRE") };
        if (!clusterType)
            throw RequestedDataNotFoundError{};

        auto cluster{ clusterType->getCluster(genre) };
        if (!cluster)
            throw RequestedDataNotFoundError{};

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& songsByGenreNode{ response.createNode("songsByGenre") };

        Track::FindParameters params;
        params.setClusters({ cluster->getId() });
        params.setRange({ offset, count });

        const auto tracks{ Track::find(context.dbSession, params) };
        for (const Track::pointer& track : tracks.results)
            songsByGenreNode.addArrayChild("song", createSongNode(context, track, user));

        return response;
    }

    Response handleGetStarredRequest(RequestContext& context)
    {
        return handleGetStarredRequestCommon(context, false /* no id3 */);
    }

    Response handleGetStarred2Request(RequestContext& context)
    {
        return handleGetStarredRequestCommon(context, true /* id3 */);
    }

}