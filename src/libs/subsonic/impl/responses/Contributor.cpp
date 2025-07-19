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

#include "responses/Contributor.hpp"

#include "database/Object.hpp"
#include "database/objects/TrackArtistLink.hpp"

#include "SubsonicResponse.hpp"
#include "responses/Artist.hpp"

namespace lms::api::subsonic
{
    Response::Node createContributorNode(const db::ObjectPtr<db::TrackArtistLink>& trackArtistLink, const db::ObjectPtr<db::Artist>& artist)
    {
        Response::Node contributorNode;

        contributorNode.setAttribute("role", utils::toString(trackArtistLink->getType()));
        if (!trackArtistLink->getSubType().empty())
            contributorNode.setAttribute("subRole", trackArtistLink->getSubType());
        contributorNode.addChild("artist", createArtistNode(artist));

        return contributorNode;
    }
} // namespace lms::api::subsonic