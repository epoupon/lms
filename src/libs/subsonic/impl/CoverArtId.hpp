/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <ctime>
#include <variant>

#include "core/String.hpp"
#include "database/ImageId.hpp"
#include "database/TrackId.hpp"

namespace lms::api::subsonic
{
    struct CoverArtId
    {
        std::variant<db::ImageId, db::TrackId> id;
        std::time_t timestamp;
    };

    std::string idToString(CoverArtId coverId);
    std::string idToString(db::ImageId imageId);
} // namespace lms::api::subsonic

// Used to parse parameters
namespace lms::core::stringUtils
{
    template<>
    std::optional<db::ImageId> readAs(std::string_view str);

    template<>
    std::optional<api::subsonic::CoverArtId> readAs(std::string_view str);
} // namespace lms::core::stringUtils
