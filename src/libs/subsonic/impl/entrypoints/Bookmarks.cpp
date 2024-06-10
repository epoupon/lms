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
#include "database/Track.hpp"
#include "database/TrackBookmark.hpp"
#include "database/User.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "responses/Bookmark.hpp"
#include "responses/Song.hpp"

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
} // namespace lms::api::subsonic