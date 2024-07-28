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

#include "responses/User.hpp"

#include "database/MediaLibrary.hpp"
#include "database/User.hpp"

#include "RequestContext.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    Response::Node createUserNode(RequestContext& context, const db::User::pointer& user)
    {
        Response::Node userNode;

        userNode.setAttribute("username", user->getLoginName());
        userNode.setAttribute("scrobblingEnabled", true);
        userNode.setAttribute("adminRole", user->isAdmin());
        userNode.setAttribute("settingsRole", true);
        userNode.setAttribute("downloadRole", true);
        userNode.setAttribute("uploadRole", false);
        userNode.setAttribute("playlistRole", true);
        userNode.setAttribute("coverArtRole", false);
        userNode.setAttribute("commentRole", false);
        userNode.setAttribute("podcastRole", false); // not supported
        userNode.setAttribute("streamRole", true);
        userNode.setAttribute("jukeboxRole", false); // not supported
        userNode.setAttribute("shareRole", false);   // not supported

        // users can access all libraries
        db::MediaLibrary::find(context.dbSession, [&](const db::MediaLibrary::pointer& library) {
            userNode.addArrayValue("folder", idToString(library->getId()));
        });

        return userNode;
    }
} // namespace lms::api::subsonic