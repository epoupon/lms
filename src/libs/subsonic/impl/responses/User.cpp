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

#include "database/objects/MediaLibrary.hpp"
#include "database/objects/User.hpp"

#include "RequestContext.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    Response::Node createUserNode(RequestContext& context, const db::User::pointer& user)
    {
        Response::Node userNode;

        userNode.setAttribute("username", user->getLoginName());
        userNode.setAttribute("scrobblingEnabled", true);
        userNode.setAttribute("adminRole", user->isAdmin());   // Whether the user is administrator
        userNode.setAttribute("settingsRole", true);           // Whether the user is allowed to change personal settings and password
        userNode.setAttribute("downloadRole", true);           // Whether the user is allowed to download files
        userNode.setAttribute("uploadRole", false);            // Whether the user is allowed to upload files
        userNode.setAttribute("playlistRole", true);           // Whether the user is allowed to create and delete playlists
        userNode.setAttribute("coverArtRole", false);          // Whether the user is allowed to change cover art and tags.
        userNode.setAttribute("commentRole", false);           // Whether the user is allowed to create and edit comments and ratings
        userNode.setAttribute("podcastRole", user->isAdmin()); // Whether the user is allowed to administrate Podcasts
        userNode.setAttribute("streamRole", true);             // Whether the user is allowed to play files
        userNode.setAttribute("jukeboxRole", false);           // not supported
        userNode.setAttribute("shareRole", false);             // not supported

        // users can access all libraries
        db::MediaLibrary::find(context.dbSession, [&](const db::MediaLibrary::pointer& library) {
            userNode.addArrayValue("folder", library->getId().getValue());
        });

        return userNode;
    }
} // namespace lms::api::subsonic