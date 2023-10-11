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
#include "services/database/Object.hpp"
#include "services/database/Types.hpp"
#include "SubsonicResponse.hpp"

namespace Database
{
    class Artist;
    class User;
    class Session;
}

namespace API::Subsonic
{
    namespace Utils
    {
        std::string joinArtistNames(const std::vector<Database::ObjectPtr<Database::Artist>>& artists);
        std::string_view toString(Database::TrackArtistLinkType type);
    }
    Response::Node createArtistNode(const Database::ObjectPtr<Database::Artist>& artist, Database::Session& session, const Database::ObjectPtr<Database::User>& user, bool id3);
    Response::Node createArtistNode(const Database::ObjectPtr<Database::Artist>& artist); // only minimal info
}