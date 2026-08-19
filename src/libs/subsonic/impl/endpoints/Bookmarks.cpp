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

namespace lms::api::subsonic
{
    Response handleGetBookmarks(RequestContext& context)
    {
        auto transaction{ context.getDbSession().createReadTransaction() };

        const auto bookmarkIds{ db::TrackBookmark::find(context.getDbSession(), context.getUser()->getId()) };

        Response response{ Response::createOkResponse() };
        Response::Node& bookmarksNode{ response.createNode("bookmarks") };

        for (const db::TrackBookmarkId bookmarkId : bookmarkIds)
        {
            const db::TrackBookmark::pointer bookmark{ db::TrackBookmark::find(context.getDbSession(), bookmarkId) };
            Response::Node bookmarkNode{ createBookmarkNode(bookmark) };
            bookmarkNode.addChild("entry", createSongNode(context, bookmark->getTrack(), context.getUser()));
            bookmarksNode.addArrayChild("bookmark", std::move(bookmarkNode));
        }

        return response;
    }

    Response handleCreateBookmark(RequestContext& context)
    {
        // Mandatory params
        db::TrackId trackId{ getMandatoryParameterAs<db::TrackId>(context.getParameters(), "id") };
        unsigned long position{ getMandatoryParameterAs<unsigned long>(context.getParameters(), "position") };
        const std::optional<std::string> comment{ getParameterAs<std::string>(context.getParameters(), "comment") };

        auto transaction{ context.getDbSession().createWriteTransaction() };

        const db::Track::pointer track{ db::Track::find(context.getDbSession(), trackId) };
        if (!track)
            throw RequestedDataNotFoundError{};

        // Replace any existing bookmark
        auto bookmark{ db::TrackBookmark::find(context.getDbSession(), context.getUser()->getId(), trackId) };
        if (!bookmark)
            bookmark = context.getDbSession().create<db::TrackBookmark>(context.getUser(), track);

        bookmark.modify()->setOffset(std::chrono::milliseconds{ position });
        if (comment)
            bookmark.modify()->setComment(*comment);

        return Response::createOkResponse();
    }

    Response handleDeleteBookmark(RequestContext& context)
    {
        // Mandatory params
        db::TrackId trackId{ getMandatoryParameterAs<db::TrackId>(context.getParameters(), "id") };

        auto transaction{ context.getDbSession().createWriteTransaction() };

        auto bookmark{ db::TrackBookmark::find(context.getDbSession(), context.getUser()->getId(), trackId) };
        if (!bookmark)
            throw RequestedDataNotFoundError{};

        bookmark.remove();

        return Response::createOkResponse();
    }

    static db::PlayQueue::pointer getOrCreatePlayQueue(RequestContext& context)
    {
        constexpr std::string_view name;

        db::PlayQueue::pointer playQueue;
        {
            auto transaction{ context.getDbSession().createReadTransaction() };

            playQueue = db::PlayQueue::find(context.getDbSession(), context.getUser()->getId(), name);
        }

        if (!playQueue)
        {
            auto transaction{ context.getDbSession().createWriteTransaction() };

            playQueue = db::PlayQueue::find(context.getDbSession(), context.getUser()->getId(), name);
            if (!playQueue)
            {
                playQueue = context.getDbSession().create<db::PlayQueue>(context.getUser(), name);
                playQueue.modify()->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
            }
        }

        return playQueue;
    }

    // PlayQueue makes use of a dedicated internal playlist
    static Response handleGetPlayQueueCommon(RequestContext& context, bool byIndex)
    {
        Response response{ Response::createOkResponse() };

        const db::PlayQueue::pointer playQueue{ getOrCreatePlayQueue(context) };
        assert(playQueue);

        auto transaction{ context.getDbSession().createReadTransaction() };

        Response::Node& playQueueNode{ response.createNode(byIndex ? core::LiteralString{ "playQueueByIndex" } : core::LiteralString{ "playQueue" }) };

        // mandatory fields
        playQueueNode.setAttribute("username", context.getUser()->getLoginName());
        playQueueNode.setAttribute("changed", core::stringUtils::toISO8601String(playQueue->getLastModifiedDateTime()));
        playQueueNode.setAttribute("changedBy", ""); // we don't store the client name (could be several same clients on several devices...)

        // optional fields
        if (!playQueue->isEmpty())
        {
            if (byIndex)
                playQueueNode.setAttribute("index", playQueue->getCurrentIndex());
            else if (const db::Track::pointer currentTrack{ playQueue->getTrackAtCurrentIndex() })
                playQueueNode.setAttribute("current", idToString(currentTrack->getId()));

            playQueueNode.setAttribute("position", std::chrono::duration_cast<std::chrono::milliseconds>(playQueue->getCurrentPositionInTrack()).count());

            playQueue->visitTracks([&](const db::Track::pointer& track) {
                playQueueNode.addArrayChild("entry", createSongNode(context, track, true /* id3 */));
            });
        }

        return response;
    }

    Response handleGetPlayQueue(RequestContext& context)
    {
        return handleGetPlayQueueCommon(context, false);
    }

    Response handleGetPlayQueueByIndex(RequestContext& context)
    {
        return handleGetPlayQueueCommon(context, true);
    }

    static Response handleSavePlayQueueCommon(RequestContext& context, bool byIndex)
    {
        std::vector<db::TrackId> trackIds{ getMultiParametersAs<db::TrackId>(context.getParameters(), "id") };
        const std::optional<db::TrackId> currentTrackId{ byIndex ? std::nullopt : getParameterAs<db::TrackId>(context.getParameters(), "current") };
        const std::optional<std::size_t> currentIndex{ byIndex ? getParameterAs<std::size_t>(context.getParameters(), "currentIndex") : std::nullopt };
        const std::chrono::milliseconds currentPositionInTrack{ getParameterAs<std::size_t>(context.getParameters(), "current").value_or(0) };

        db::PlayQueue::pointer playQueue{ getOrCreatePlayQueue(context) };
        assert(playQueue);

        {
            auto transaction{ context.getDbSession().createWriteTransaction() };

            // no id means we clear the play queue (see https://github.com/opensubsonic/open-subsonic-api/pull/106)
            playQueue.modify()->clear();

            std::size_t currentPlayQueueIndex{};
            std::size_t currentTrackIndex{};
            for (db::TrackId trackId : trackIds)
            {
                if (const db::Track::pointer track{ db::Track::find(context.getDbSession(), trackId) })
                {
                    playQueue.modify()->addTrack(track);

                    if ((currentTrackId && *currentTrackId == track->getId())
                        || (currentIndex && *currentIndex == currentTrackIndex))
                    {
                        playQueue.modify()->setCurrentIndex(currentPlayQueueIndex);
                    }

                    currentPlayQueueIndex++;
                }

                currentTrackIndex++;
            }

            playQueue.modify()->setCurrentPositionInTrack(currentPositionInTrack);
            playQueue.modify()->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
        }

        return Response::createOkResponse();
    }

    Response handleSavePlayQueue(RequestContext& context)
    {
        return handleSavePlayQueueCommon(context, false);
    }

    Response handleSavePlayQueueByIndex(RequestContext& context)
    {
        return handleSavePlayQueueCommon(context, true);
    }
} // namespace lms::api::subsonic