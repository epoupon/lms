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

#include "Playlists.hpp"

#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/User.hpp"
#include "responses/Playlist.hpp"
#include "responses/Song.hpp"
#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"

namespace API::Subsonic
{
    using namespace Database;

    Response handleGetPlaylistsRequest(RequestContext& context)
    {
        auto transaction{ context.dbSession.createSharedTransaction() };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& playlistsNode{ response.createNode("playlists") };

        TrackList::FindParameters params;
        params.setUser(context.userId);
        params.setType(TrackListType::Playlist);

        auto tracklistIds{ TrackList::find(context.dbSession, params) };
        for (const TrackListId trackListId : tracklistIds.results)
        {
            const TrackList::pointer trackList{ TrackList::find(context.dbSession, trackListId) };
            playlistsNode.addArrayChild("playlist", createPlaylistNode(trackList, context.dbSession));
        }

        return response;
    }

    Response handleGetPlaylistRequest(RequestContext& context)
    {
        // Mandatory params
        TrackListId trackListId{ getMandatoryParameterAs<TrackListId>(context.parameters, "id") };

        auto transaction{ context.dbSession.createSharedTransaction() };

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        TrackList::pointer tracklist{ TrackList::find(context.dbSession, trackListId) };
        if (!tracklist)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node playlistNode{ createPlaylistNode(tracklist, context.dbSession) };

        auto entries{ tracklist->getEntries() };
        for (const TrackListEntry::pointer& entry : entries)
            playlistNode.addArrayChild("entry", createSongNode(entry->getTrack(), context.dbSession, user));

        response.addNode("playlist", std::move(playlistNode));

        return response;
    }

    Response handleCreatePlaylistRequest(RequestContext& context)
    {
        // Optional params
        const auto id{ getParameterAs<TrackListId>(context.parameters, "playlistId") };
        auto name{ getParameterAs<std::string>(context.parameters, "name") };

        std::vector<TrackId> trackIds{ getMultiParametersAs<TrackId>(context.parameters, "songId") };

        if (!name && !id)
            throw RequiredParameterMissingError{ "name or id" };

        auto transaction{ context.dbSession.createUniqueTransaction() };

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        TrackList::pointer tracklist;
        if (id)
        {
            tracklist = TrackList::find(context.dbSession, *id);
            if (!tracklist
                || tracklist->getUser() != user
                || tracklist->getType() != TrackListType::Playlist)
            {
                throw RequestedDataNotFoundError{};
            }

            if (name)
                tracklist.modify()->setName(*name);
        }
        else
        {
            tracklist = context.dbSession.create<TrackList>(*name, TrackListType::Playlist, false, user);
        }

        for (const TrackId trackId : trackIds)
        {
            Track::pointer track{ Track::find(context.dbSession, trackId) };
            if (!track)
                continue;

            context.dbSession.create<TrackListEntry>(track, tracklist);
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleUpdatePlaylistRequest(RequestContext& context)
    {
        // Mandatory params
        TrackListId id{ getMandatoryParameterAs<TrackListId>(context.parameters, "playlistId") };

        // Optional parameters
        auto name{ getParameterAs<std::string>(context.parameters, "name") };
        auto isPublic{ getParameterAs<bool>(context.parameters, "public") };

        std::vector<TrackId> trackIdsToAdd{ getMultiParametersAs<TrackId>(context.parameters, "songIdToAdd") };
        std::vector<std::size_t> trackPositionsToRemove{ getMultiParametersAs<std::size_t>(context.parameters, "songIndexToRemove") };

        auto transaction{ context.dbSession.createUniqueTransaction() };

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        TrackList::pointer tracklist{ TrackList::find(context.dbSession, id) };
        if (!tracklist
            || tracklist->getUser() != user
            || tracklist->getType() != TrackListType::Playlist)
        {
            throw RequestedDataNotFoundError{};
        }

        if (name)
            tracklist.modify()->setName(*name);

        if (isPublic)
            tracklist.modify()->setIsPublic(*isPublic);

        {
            // Remove from end to make indexes stable
            std::sort(std::begin(trackPositionsToRemove), std::end(trackPositionsToRemove), std::greater<std::size_t>());

            for (std::size_t trackPositionToRemove : trackPositionsToRemove)
            {
                auto entry{ tracklist->getEntry(trackPositionToRemove) };
                if (entry)
                    entry.remove();
            }
        }

        // Add tracks
        for (const TrackId trackIdToAdd : trackIdsToAdd)
        {
            Track::pointer track{ Track::find(context.dbSession, trackIdToAdd) };
            if (!track)
                continue;

            context.dbSession.create<TrackListEntry>(track, tracklist);
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleDeletePlaylistRequest(RequestContext& context)
    {
        TrackListId id{ getMandatoryParameterAs<TrackListId>(context.parameters, "id") };

        auto transaction{ context.dbSession.createUniqueTransaction() };

        User::pointer user{ User::find(context.dbSession, context.userId) };
        if (!user)
            throw UserNotAuthorizedError{};

        TrackList::pointer tracklist{ TrackList::find(context.dbSession, id) };
        if (!tracklist
            || tracklist->getUser() != user
            || tracklist->getType() != TrackListType::Playlist)
        {
            throw RequestedDataNotFoundError{};
        }

        tracklist.remove();

        return Response::createOkResponse(context.serverProtocolVersion);
    }
}