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

#pragma once

#include <string>
#include <vector>

#include "database/Object.hpp"
#include "database/Types.hpp"

#include "SubsonicResponse.hpp"

namespace lms::db
{
    class Artist;
    class User;
    class Session;
} // namespace lms::db

namespace lms::api::subsonic
{
    struct RequestContext;

    namespace utils
    {
        std::string joinArtistNames(const std::vector<db::ObjectPtr<db::Artist>>& artists);
        std::string_view toString(db::TrackArtistLinkType type);
    } // namespace utils

    Response::Node createArtistNode(RequestContext& context, const db::ObjectPtr<db::Artist>& artist);
    Response::Node createArtistNode(const db::ObjectPtr<db::Artist>& artist); // only minimal info
} // namespace lms::api::subsonic