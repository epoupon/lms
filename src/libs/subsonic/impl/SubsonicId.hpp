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
#include "database/ArtistId.hpp"
#include "database/DirectoryId.hpp"
#include "database/MediaLibraryId.hpp"
#include "database/ReleaseId.hpp"
#include "database/TrackId.hpp"
#include "database/TrackListId.hpp"

namespace lms::api::subsonic
{
    struct RootId
    {
    };

    std::string idToString(db::ArtistId id);
    std::string idToString(db::DirectoryId id);
    std::string idToString(db::MediaLibraryId id);
    std::string idToString(db::ReleaseId id);
    std::string idToString(db::TrackId id);
    std::string idToString(db::TrackListId id);
    std::string idToString(RootId);
} // namespace lms::api::subsonic

// Used to parse parameters
namespace lms::core::stringUtils
{
    template<>
    std::optional<api::subsonic::RootId> readAs(std::string_view str);

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
