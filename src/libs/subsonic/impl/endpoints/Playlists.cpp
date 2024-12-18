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

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "responses/Playlist.hpp"
#include "responses/Song.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    Response handleGetPlaylistsRequest(RequestContext& context)
    {
        auto transaction{ context.dbSession.createReadTransaction() };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& playlistsNode{ response.createNode("playlists") };

        auto addTrackList{ [&](const db::TrackList::pointer& trackList) {
            playlistsNode.addArrayChild("playlist", createPlaylistNode(trackList, context.dbSession));
        } };

        // First add user's playlists
        {
            TrackList::FindParameters params;
            params.setUser(context.user->getId());
            params.setType(TrackListType::PlayList);

            db::TrackList::find(context.dbSession, params, [&](const db::TrackList::pointer& trackList) {
                addTrackList(trackList);
            });
        }

        // Then add others public playlists
        {
            TrackList::FindParameters params;
            params.setVisibility(TrackList::Visibility::Public);
            params.setType(TrackListType::PlayList);

            db::TrackList::find(context.dbSession, params, [&](const db::TrackList::pointer& trackList) {
                if (trackList->getUserId() != context.user->getId()) // skip already reported
                    addTrackList(trackList);
            });
        }

        return response;
    }

    Response handleGetPlaylistRequest(RequestContext& context)
    {
        // Mandatory params
        TrackListId trackListId{ getMandatoryParameterAs<TrackListId>(context.parameters, "id") };

        auto transaction{ context.dbSession.createReadTransaction() };

        TrackList::pointer tracklist{ TrackList::find(context.dbSession, trackListId) };
        if (!tracklist)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node playlistNode{ createPlaylistNode(tracklist, context.dbSession) };

        auto entries{ tracklist->getEntries() };
        for (const TrackListEntry::pointer& entry : entries.results)
            playlistNode.addArrayChild("entry", createSongNode(context, entry->getTrack(), context.user));

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
            throw RequiredParameterMissingError{ "name or playlistId" };

        auto transaction{ context.dbSession.createWriteTransaction() };

        TrackList::pointer tracklist;
        if (id)
        {
            tracklist = TrackList::find(context.dbSession, *id);
            if (!tracklist
                || tracklist->getUser() != context.user
                || tracklist->getType() != TrackListType::PlayList)
            {
                throw RequestedDataNotFoundError{};
            }

            if (name)
                tracklist.modify()->setName(*name);

            tracklist.modify()->clear();
            tracklist.modify()->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
        }
        else
        {
            tracklist = context.dbSession.create<TrackList>(*name, TrackListType::PlayList);
            tracklist.modify()->setUser(context.user);
            tracklist.modify()->setVisibility(TrackList::Visibility::Private);
        }

        for (const TrackId trackId : trackIds)
        {
            Track::pointer track{ Track::find(context.dbSession, trackId) };
            if (!track)
                continue;

            context.dbSession.create<TrackListEntry>(track, tracklist);
        }

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node playlistNode{ createPlaylistNode(tracklist, context.dbSession) };

        auto entries{ tracklist->getEntries() };
        for (const TrackListEntry::pointer& entry : entries.results)
            playlistNode.addArrayChild("entry", createSongNode(context, entry->getTrack(), context.user));

        response.addNode("playlist", std::move(playlistNode));

        return response;
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

        auto transaction{ context.dbSession.createWriteTransaction() };

        TrackList::pointer tracklist{ TrackList::find(context.dbSession, id) };
        if (!tracklist
            || tracklist->getUser() != context.user
            || tracklist->getType() != TrackListType::PlayList)
        {
            throw RequestedDataNotFoundError{};
        }

        if (name)
            tracklist.modify()->setName(*name);

        tracklist.modify()->setVisibility(isPublic ? db::TrackList::Visibility::Public : db::TrackList::Visibility::Private);
        tracklist.modify()->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());

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

        auto transaction{ context.dbSession.createWriteTransaction() };

        TrackList::pointer tracklist{ TrackList::find(context.dbSession, id) };
        if (!tracklist
            || tracklist->getUser() != context.user
            || tracklist->getType() != TrackListType::PlayList)
        {
            throw RequestedDataNotFoundError{};
        }

        tracklist.remove();

        return Response::createOkResponse(context.serverProtocolVersion);
    }
} // namespace lms::api::subsonic