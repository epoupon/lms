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

#include "Bookmarks.hpp"

#include "database/Session.hpp"
#include "database/objects/PlayQueue.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackBookmark.hpp"
#include "database/objects/User.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "responses/Bookmark.hpp"
#include "responses/Song.hpp"

#include "core/String.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    Response handleGetBookmarks(RequestContext& context)
    {
        auto transaction{ context.dbSession.createReadTransaction() };

        const auto bookmarkIds{ TrackBookmark::find(context.dbSession, context.user->getId()) };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& bookmarksNode{ response.createNode("bookmarks") };

        for (const TrackBookmarkId bookmarkId : bookmarkIds.results)
        {
            const TrackBookmark::pointer bookmark{ TrackBookmark::find(context.dbSession, bookmarkId) };
            Response::Node bookmarkNode{ createBookmarkNode(bookmark) };
            bookmarkNode.addChild("entry", createSongNode(context, bookmark->getTrack(), context.user));
            bookmarksNode.addArrayChild("bookmark", std::move(bookmarkNode));
        }

        return response;
    }

    Response handleCreateBookmark(RequestContext& context)
    {
        // Mandatory params
        TrackId trackId{ getMandatoryParameterAs<TrackId>(context.parameters, "id") };
        unsigned long position{ getMandatoryParameterAs<unsigned long>(context.parameters, "position") };
        const std::optional<std::string> comment{ getParameterAs<std::string>(context.parameters, "comment") };

        auto transaction{ context.dbSession.createWriteTransaction() };

        const Track::pointer track{ Track::find(context.dbSession, trackId) };
        if (!track)
            throw RequestedDataNotFoundError{};

        // Replace any existing bookmark
        auto bookmark{ TrackBookmark::find(context.dbSession, context.user->getId(), trackId) };
        if (!bookmark)
            bookmark = context.dbSession.create<TrackBookmark>(context.user, track);

        bookmark.modify()->setOffset(std::chrono::milliseconds{ position });
        if (comment)
            bookmark.modify()->setComment(*comment);

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleDeleteBookmark(RequestContext& context)
    {
        // Mandatory params
        TrackId trackId{ getMandatoryParameterAs<TrackId>(context.parameters, "id") };

        auto transaction{ context.dbSession.createWriteTransaction() };

        auto bookmark{ TrackBookmark::find(context.dbSession, context.user->getId(), trackId) };
        if (!bookmark)
            throw RequestedDataNotFoundError{};

        bookmark.remove();

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    // Use a dedicated internal playlist
    Response handleGetPlayQueue(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };

        auto transaction{ context.dbSession.createReadTransaction() };
        const db::PlayQueue::pointer playQueue{ db::PlayQueue::find(context.dbSession, context.user->getId(), "subsonic") };
        if (playQueue)
        {
            Response::Node& playQueueNode{ response.createNode("playQueue") };
            if (auto currentTrack{ playQueue->getTrackAtCurrentIndex() })
            {
                // optional fields
                playQueueNode.setAttribute("current", idToString(currentTrack->getId()));
                playQueueNode.setAttribute("position", playQueue->getCurrentPositionInTrack().count());
            }

            // mandatory fields
            playQueueNode.setAttribute("username", context.user->getLoginName());
            playQueueNode.setAttribute("changed", core::stringUtils::toISO8601String(playQueue->getLastModifiedDateTime()));
            playQueueNode.setAttribute("changedBy", "unknown"); // we don't store the client name (could be several same clients on several devices...)

            playQueue->visitTracks([&](const db::Track::pointer& track) {
                playQueueNode.addArrayChild("entry", createSongNode(context, track, true /* id3 */));
            });
        }

        return response;
    }

    Response handleSavePlayQueue(RequestContext& context)
    {
        // optional params
        std::vector<db::TrackId> trackIds{ getMultiParametersAs<TrackId>(context.parameters, "id") };
        const std::optional<db::TrackId> currentTrackId{ getParameterAs<db::TrackId>(context.parameters, "current") };
        const std::chrono::milliseconds currentPositionInTrack{ getParameterAs<std::size_t>(context.parameters, "current").value_or(0) };

        std::vector<db::Track::pointer> tracks;
        tracks.reserve(trackIds.size());

        // no id means we clear the play queue (see https://github.com/opensubsonic/open-subsonic-api/pull/106)
        if (!trackIds.empty())
        {
            auto transaction{ context.dbSession.createReadTransaction() };
            for (db::TrackId trackId : trackIds)
            {
                if (db::Track::pointer track{ db::Track::find(context.dbSession, trackId) })
                    tracks.push_back(track);
            }
        }

        {
            auto transaction{ context.dbSession.createWriteTransaction() };

            db::PlayQueue::pointer playQueue{ db::PlayQueue::find(context.dbSession, context.user->getId(), "subsonic") };
            if (!playQueue)
                playQueue = context.dbSession.create<db::PlayQueue>(context.user, "subsonic");

            playQueue.modify()->clear();
            std::size_t index{};
            for (std::size_t i{}; i < tracks.size(); ++i)
            {
                db::Track::pointer& track{ tracks[i] };
                playQueue.modify()->addTrack(track);

                if (track->getId() == currentTrackId)
                    index = i;
            }

            playQueue.modify()->setCurrentIndex(index);
            playQueue.modify()->setCurrentPositionInTrack(currentPositionInTrack);
            playQueue.modify()->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }
} // namespace lms::api::subsonic