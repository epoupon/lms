/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "services/database/ArtistId.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/TrackId.hpp"
#include "services/database/TrackListId.hpp"
#include "utils/String.hpp"

namespace API::Subsonic
{
    struct RootId {};

    std::string idToString(Database::ArtistId id);
    std::string idToString(Database::ReleaseId id);
    std::string idToString(Database::TrackId id);
    std::string idToString(Database::TrackListId id);
    std::string idToString(RootId);
} // namespace API::Subsonic

// Used to parse parameters
namespace StringUtils
{
    template<>
    std::optional<API::Subsonic::RootId> readAs(std::string_view str);

    template<>
    std::optional<Database::ArtistId> readAs(std::string_view str);

    template<>
    std::optional<Database::ReleaseId> readAs(std::string_view str);

    template<>
    std::optional<Database::TrackId> readAs(std::string_view str);

    template<>
    std::optional<Database::TrackListId> readAs(std::string_view str);
}

