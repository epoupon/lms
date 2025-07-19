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
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "responses/Playlist.hpp"
#include "responses/Song.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace
    {
        void checkTrackListModificationAccess(const db::TrackList::pointer& trackList, const db::UserId currentUserId)
        {
            if (!trackList || trackList->getType() != TrackListType::PlayList)
                throw RequestedDataNotFoundError{};

            // Can only modify own playlists
            if (trackList->getUserId() != currentUserId)
            {
                if (trackList->getVisibility() == TrackList::Visibility::Public)
                    throw UserNotAuthorizedError{};

                throw RequestedDataNotFoundError{};
            }
        }
    } // namespace

    Response handleGetPlaylistsRequest(RequestContext& context)
    {
        auto transaction{ context.dbSession.createReadTransaction() };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& playlistsNode{ response.createNode("playlists") };

        auto addTrackList{ [&](const db::TrackList::pointer& trackList) {
            playlistsNode.addArrayChild("playlist", createPlaylistNode(context, trackList));
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
            params.setExcludedUser(context.user->getId());

            db::TrackList::find(context.dbSession, params, [&](const db::TrackList::pointer& trackList) {
                assert(trackList->getUserId() != context.user->getId());
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

        TrackList::pointer trackList{ TrackList::find(context.dbSession, trackListId) };
        if (!trackList || trackList->getType() != TrackListType::PlayList)
            throw RequestedDataNotFoundError{};

        if (trackList->getUserId() != context.user->getId() && trackList->getVisibility() != TrackList::Visibility::Public)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node playlistNode{ createPlaylistNode(context, trackList) };

        auto entries{ trackList->getEntries() };
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

        TrackList::pointer trackList;
        if (id)
        {
            trackList = TrackList::find(context.dbSession, *id);
            checkTrackListModificationAccess(trackList, context.user->getId());

            if (name)
                trackList.modify()->setName(*name);

            trackList.modify()->clear();
            trackList.modify()->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
        }
        else
        {
            trackList = context.dbSession.create<TrackList>(*name, TrackListType::PlayList);
            trackList.modify()->setUser(context.user);
            trackList.modify()->setVisibility(TrackList::Visibility::Private);
        }

        for (const TrackId trackId : trackIds)
        {
            Track::pointer track{ Track::find(context.dbSession, trackId) };
            if (!track)
                continue;

            context.dbSession.create<TrackListEntry>(track, trackList);
        }

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node playlistNode{ createPlaylistNode(context, trackList) };

        auto entries{ trackList->getEntries() };
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

        TrackList::pointer trackList{ TrackList::find(context.dbSession, id) };
        checkTrackListModificationAccess(trackList, context.user->getId());

        if (name)
            trackList.modify()->setName(*name);

        trackList.modify()->setVisibility(isPublic ? db::TrackList::Visibility::Public : db::TrackList::Visibility::Private);
        trackList.modify()->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());

        {
            // Remove from end to make indexes stable
            std::sort(std::begin(trackPositionsToRemove), std::end(trackPositionsToRemove), std::greater<std::size_t>());

            for (std::size_t trackPositionToRemove : trackPositionsToRemove)
            {
                auto entry{ trackList->getEntry(trackPositionToRemove) };
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

            context.dbSession.create<TrackListEntry>(track, trackList);
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleDeletePlaylistRequest(RequestContext& context)
    {
        TrackListId id{ getMandatoryParameterAs<TrackListId>(context.parameters, "id") };

        auto transaction{ context.dbSession.createWriteTransaction() };

        TrackList::pointer trackList{ TrackList::find(context.dbSession, id) };
        checkTrackListModificationAccess(trackList, context.user->getId());

        trackList.remove();

        return Response::createOkResponse(context.serverProtocolVersion);
    }
} // namespace lms::api::subsonic