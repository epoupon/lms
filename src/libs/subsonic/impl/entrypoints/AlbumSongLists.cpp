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
#include "services/scrobbling/IScrobblingService.hpp"
#include "responses/Album.hpp"
#include "responses/Artist.hpp"
#include "responses/Song.hpp"
#include "utils/Service.hpp"
#include "ParameterParsing.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace {
        Response handleGetAlbumListRequestCommon(const RequestContext& context, bool id3)
        {
            // Mandatory params
            const std::string type{ getMandatoryParameterAs<std::string>(context.parameters, "type") };

            // Optional params
            const std::size_t size{ getParameterAs<std::size_t>(context.parameters, "size").value_or(10) };
            const std::size_t offset{ getParameterAs<std::size_t>(context.parameters, "offset").value_or(0) };

            const Range range{ offset, size };

            RangeResults<ReleaseId> releases;
            Scrobbling::IScrobblingService& scrobbling{ *Service<Scrobbling::IScrobblingService>::get() };

            auto transaction{ context.dbSession.createSharedTransaction() };

            User::pointer user{ User::find(context.dbSession, context.userId) };
            if (!user)
                throw UserNotAuthorizedError{};

            if (type == "alphabeticalByName")
            {
                Release::FindParameters params;
                params.setSortMethod(ReleaseSortMethod::Name);
                params.setRange(range);

                releases = Release::find(context.dbSession, params);
            }
            else if (type == "alphabeticalByArtist")
            {
                releases = Release::findOrderedByArtist(context.dbSession, range);
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

                        releases = Release::find(context.dbSession, params);
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

                releases = Release::find(context.dbSession, params);
            }
            else if (type == "frequent")
            {
                releases = scrobbling.getTopReleases(context.userId, {}, range);
            }
            else if (type == "newest")
            {
                Release::FindParameters params;
                params.setSortMethod(ReleaseSortMethod::LastWritten);
                params.setRange(range);

                releases = Release::find(context.dbSession, params);
            }
            else if (type == "random")
            {
                // Random results are paginated, but there is no acceptable way to handle the pagination params without repeating some albums
                // (no seed provided by subsonic, ot it would require to store some kind of context for each user/client when iterating over the random albums)
                Release::FindParameters params;
                params.setSortMethod(ReleaseSortMethod::Random);
                params.setRange({ 0, size });

                releases = Release::find(context.dbSession, params);
            }
            else if (type == "recent")
            {
                releases = scrobbling.getRecentReleases(context.userId, {}, range);
            }
            else if (type == "starred")
            {
                releases = scrobbling.getStarredReleases(context.userId, {}, range);
            }
            else
                throw NotImplementedGenericError{};

            Response response{ Response::createOkResponse(context.serverProtocolVersion) };
            Response::Node& albumListNode{ response.createNode(id3 ? Response::Node::Key{ "albumList2" } : Response::Node::Key{ "albumList" }) };

            for (const ReleaseId releaseId : releases.results)
            {
                const Release::pointer release{ Release::find(context.dbSession, releaseId) };
                albumListNode.addArrayChild("album", createAlbumNode(release, context.dbSession, user, id3));
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

            Scrobbling::IScrobblingService& scrobbling{ *Service<Scrobbling::IScrobblingService>::get() };

            for (const ArtistId artistId : scrobbling.getStarredArtists(context.userId, {} /* clusters */, std::nullopt /* linkType */, ArtistSortMethod::BySortName, Range{}).results)
            {
                if (auto artist{ Artist::find(context.dbSession, artistId) })
                    starredNode.addArrayChild("artist", createArtistNode(artist, context.dbSession, user, id3));
            }

            for (const ReleaseId releaseId : scrobbling.getStarredReleases(context.userId, {} /* clusters */, Range{}).results)
            {
                if (auto release{ Release::find(context.dbSession, releaseId) })
                    starredNode.addArrayChild("album", createAlbumNode(release, context.dbSession, user, id3));
            }

            for (const TrackId trackId : scrobbling.getStarredTracks(context.userId, {} /* clusters */, Range{}).results)
            {
                if (auto track{ Track::find(context.dbSession, trackId) })
                    starredNode.addArrayChild("song", createSongNode(track, context.dbSession, user));
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
        size = std::min(size, std::size_t{ 500 });

        auto transaction{ context.dbSession.createSharedTransaction() };

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        const auto trackIds{ Track::find(context.dbSession, Track::FindParameters {}.setSortMethod(TrackSortMethod::Random).setRange({0, size})) };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };

        Response::Node& randomSongsNode{ response.createNode("randomSongs") };
        for (const TrackId trackId : trackIds.results)
        {
            const Track::pointer track{ Track::find(context.dbSession, trackId) };
            randomSongsNode.addArrayChild("song", createSongNode(track, context.dbSession, user));
        }

        return response;
    }

    Response handleGetSongsByGenreRequest(RequestContext& context)
    {
        // Mandatory params
        std::string genre{ getMandatoryParameterAs<std::string>(context.parameters, "genre") };

        // Optional params
        std::size_t size{ getParameterAs<std::size_t>(context.parameters, "count").value_or(10) };
        size = std::min(size, std::size_t{ 500 });

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
        params.setRange({ offset, size });

        auto trackIds{ Track::find(context.dbSession, params) };
        for (const TrackId trackId : trackIds.results)
        {
            const Track::pointer track{ Track::find(context.dbSession, trackId) };
            songsByGenreNode.addArrayChild("song", createSongNode(track, context.dbSession, user));
        }

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