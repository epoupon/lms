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

#include "core/String.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/DirectoryId.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"

namespace lms::api::subsonic
{
    std::string idToString(db::ArtistId id);
    std::string idToString(db::DirectoryId id);
    std::string idToString(db::ReleaseId id);
    std::string idToString(db::TrackId id);
    std::string idToString(db::TrackListId id);
} // namespace lms::api::subsonic

// Used to parse parameters
namespace lms::core::stringUtils
{
    template<>
    std::optional<db::ArtistId> readAs(std::string_view str);

    template<>
    std::optional<db::DirectoryId> readAs(std::string_view str);

    template<>
    std::optional<db::MediaLibraryId> readAs(std::string_view str);

    template<>
    std::optional<db::ReleaseId> readAs(std::string_view str);

    template<>
    std::optional<db::TrackId> readAs(std::string_view str);

    template<>
    std::optional<db::TrackListId> readAs(std::string_view str);
} // namespace lms::core::stringUtils
