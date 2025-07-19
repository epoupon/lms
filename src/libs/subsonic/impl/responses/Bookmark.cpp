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

#include "responses/Bookmark.hpp"

#include "database/objects/TrackBookmark.hpp"
#include "database/objects/User.hpp"

namespace lms::api::subsonic
{
    static const std::string_view reportedDummyDate{ "2000-01-01T00:00:00" };

    Response::Node createBookmarkNode(const db::ObjectPtr<db::TrackBookmark>& trackBookmark)
    {
        Response::Node trackBookmarkNode;

        trackBookmarkNode.setAttribute("position", trackBookmark->getOffset().count());
        if (!trackBookmark->getComment().empty())
            trackBookmarkNode.setAttribute("comment", trackBookmark->getComment());
        trackBookmarkNode.setAttribute("created", reportedDummyDate);
        trackBookmarkNode.setAttribute("changed", reportedDummyDate);
        trackBookmarkNode.setAttribute("username", trackBookmark->getUser()->getLoginName());

        return trackBookmarkNode;
    }
} // namespace lms::api::subsonic