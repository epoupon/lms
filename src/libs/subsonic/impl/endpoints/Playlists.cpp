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

#include <array>

#include "database/ISession.hpp"
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
        void checkTrackListModificationAccess(const std::optional<db::TrackList>& trackList, const db::UserId currentUserId)
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

        void addEntryNodes(RequestContext& context, Response::Node& playlistNode, TrackListId trackListId)
        {
            std::vector<TrackId> trackIds;
            TrackListEntry::find(context.getDbSession(), TrackListEntry::FindParameters{}.setTrackList(trackListId), [&](const TrackListEntry& entry) {
                trackIds.push_back(entry.getTrackId());
            });

            for (const TrackId trackId : trackIds)
            {
                if (const std::optional<Track> track{ Track::find(context.getDbSession(), trackId) })
                    playlistNode.addArrayChild("entry", createSongNode(context, *track, true /* id3 */));
            }
        }
    } // namespace

    Response handleGetPlaylistsRequest(RequestContext& context)
    {
        auto transaction{ context.getDbSession().createReadTransaction() };

        Response response{ Response::createOkResponse() };
        Response::Node& playlistsNode{ response.createNode("playlists") };

        auto addTrackList{ [&](const db::TrackList& trackList) {
            playlistsNode.addArrayChild("playlist", createPlaylistNode(context, trackList));
        } };

        // First add user's playlists
        {
            TrackList::FindParameters params;
            params.setUser(context.getUser()->getId());
            params.setType(TrackListType::PlayList);

            db::TrackList::find(context.getDbSession(), params, [&](const db::TrackList& trackList) {
                addTrackList(trackList);
            });
        }

        // Then add others public playlists
        {
            TrackList::FindParameters params;
            params.setVisibility(TrackList::Visibility::Public);
            params.setType(TrackListType::PlayList);
            params.setExcludedUser(context.getUser()->getId());

            db::TrackList::find(context.getDbSession(), params, [&](const db::TrackList& trackList) {
                assert(trackList.getUserId() != context.getUser()->getId());
                addTrackList(trackList);
            });
        }

        return response;
    }

    Response handleGetPlaylistRequest(RequestContext& context)
    {
        // Mandatory params
        TrackListId trackListId{ getMandatoryParameterAs<TrackListId>(context.getParameters(), "id") };

        auto transaction{ context.getDbSession().createReadTransaction() };

        std::optional<TrackList> trackList{ TrackList::find(context.getDbSession(), trackListId) };
        if (!trackList || trackList->getType() != TrackListType::PlayList)
            throw RequestedDataNotFoundError{};

        if (trackList->getUserId() != context.getUser()->getId() && trackList->getVisibility() != TrackList::Visibility::Public)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse() };
        Response::Node playlistNode{ createPlaylistNode(context, *trackList) };
        addEntryNodes(context, playlistNode, trackList->getId());

        response.addNode("playlist", std::move(playlistNode));

        return response;
    }

    Response handleCreatePlaylistRequest(RequestContext& context)
    {
        // Optional params
        const auto id{ getParameterAs<TrackListId>(context.getParameters(), "playlistId") };
        auto name{ getParameterAs<std::string>(context.getParameters(), "name") };

        std::vector<TrackId> trackIds{ getMultiParametersAs<TrackId>(context.getParameters(), "songId") };

        if (!name && !id)
            throw RequiredParameterMissingError{ "name or playlistId" };

        auto transaction{ context.getDbSession().createWriteTransaction() };

        std::optional<TrackList> trackList;
        if (id)
        {
            trackList = TrackList::find(context.getDbSession(), *id);
            checkTrackListModificationAccess(trackList, context.getUser()->getId());

            if (name)
                trackList->setName(*name);

            TrackList::clear(context.getDbSession(), trackList->getId());
            trackList->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
        }
        else
        {
            trackList = TrackList::create(context.getDbSession(), *name, TrackListType::PlayList);
            trackList->setUser(context.getUser()->getId());
            trackList->setVisibility(TrackList::Visibility::Private);
        }
        TrackList::update(context.getDbSession(), *trackList);

        for (const TrackId trackId : trackIds)
        {
            std::optional<Track> track{ Track::find(context.getDbSession(), trackId) };
            if (!track)
                continue;

            TrackListEntry::create(context.getDbSession(), track->getId(), trackList->getId());
        }

        Response response{ Response::createOkResponse() };
        Response::Node playlistNode{ createPlaylistNode(context, *trackList) };
        addEntryNodes(context, playlistNode, trackList->getId());

        response.addNode("playlist", std::move(playlistNode));

        return response;
    }

    Response handleUpdatePlaylistRequest(RequestContext& context)
    {
        // Mandatory params
        TrackListId id{ getMandatoryParameterAs<TrackListId>(context.getParameters(), "playlistId") };

        // Optional parameters
        auto name{ getParameterAs<std::string>(context.getParameters(), "name") };
        auto isPublic{ getParameterAs<bool>(context.getParameters(), "public") };

        std::vector<TrackId> trackIdsToAdd{ getMultiParametersAs<TrackId>(context.getParameters(), "songIdToAdd") };
        std::vector<std::size_t> trackPositionsToRemove{ getMultiParametersAs<std::size_t>(context.getParameters(), "songIndexToRemove") };

        auto transaction{ context.getDbSession().createWriteTransaction() };

        std::optional<TrackList> trackList{ TrackList::find(context.getDbSession(), id) };
        checkTrackListModificationAccess(trackList, context.getUser()->getId());

        if (name)
            trackList->setName(*name);

        if (isPublic)
            trackList->setVisibility(*isPublic ? db::TrackList::Visibility::Public : db::TrackList::Visibility::Private);
        trackList->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
        TrackList::update(context.getDbSession(), *trackList);

        {
            // Remove from end to make indexes stable
            std::sort(std::begin(trackPositionsToRemove), std::end(trackPositionsToRemove), std::greater<std::size_t>());

            for (std::size_t trackPositionToRemove : trackPositionsToRemove)
            {
                std::optional<TrackListEntry> entry;
                TrackListEntry::find(context.getDbSession(), TrackListEntry::FindParameters{}.setTrackList(id).setRange(Range{ trackPositionToRemove, 1 }), [&](const TrackListEntry& foundEntry) {
                    entry = foundEntry;
                });
                if (entry)
                    TrackListEntry::destroy(context.getDbSession(), std::array{ entry->getId() });
            }
        }

        // Add tracks
        for (const TrackId trackIdToAdd : trackIdsToAdd)
        {
            std::optional<Track> track{ Track::find(context.getDbSession(), trackIdToAdd) };
            if (!track)
                continue;

            TrackListEntry::create(context.getDbSession(), track->getId(), trackList->getId());
        }

        return Response::createOkResponse();
    }

    Response handleDeletePlaylistRequest(RequestContext& context)
    {
        TrackListId id{ getMandatoryParameterAs<TrackListId>(context.getParameters(), "id") };

        auto transaction{ context.getDbSession().createWriteTransaction() };

        std::optional<TrackList> trackList{ TrackList::find(context.getDbSession(), id) };
        checkTrackListModificationAccess(trackList, context.getUser()->getId());

        TrackList::destroy(context.getDbSession(), std::span{ &id, 1 });

        return Response::createOkResponse();
    }
} // namespace lms::api::subsonic