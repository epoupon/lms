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

#include "CoverArtId.hpp"

#include <cassert>

#include "core/String.hpp"
#include "database/objects/ArtworkId.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        constexpr char timestampSeparatorChar{ '-' };

        std::string idToString(db::ArtworkId id)
        {
            return "art-" + id.toString();
        }
    } // namespace

    std::string idToString(CoverArtId coverId)
    {
        std::string res;

        // produce "art-id-timestamp"
        res = idToString(coverId.id);
        res += timestampSeparatorChar;
        res += std::to_string(coverId.timestamp);

        assert(!res.empty());
        return res;
    }
} // namespace lms::api::subsonic

// Used to parse parameters
namespace lms::core::stringUtils
{
    template<>
    std::optional<api::subsonic::CoverArtId> readAs(std::string_view str)
    {
        std::optional<api::subsonic::CoverArtId> res;

        std::vector<std::string_view> values{ core::stringUtils::splitString(str, '-') };

        // expect "art-id-timestamp"
        if (values.size() != 3)
            return res;

        if (values[0] != "art")
            return res;

        const auto value{ core::stringUtils::readAs<db::ArtworkId::ValueType>(values[1]) };
        const auto timestamp{ core::stringUtils::readAs<std::time_t>(values[2]) };
        if (!value || !timestamp)
            return std::nullopt;

        res.emplace(api::subsonic::CoverArtId{ *value, *timestamp });

        return res;
    }
} // namespace lms::core::stringUtils
