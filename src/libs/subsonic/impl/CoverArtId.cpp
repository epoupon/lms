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

#include "SubsonicId.hpp"
#include "core/String.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        constexpr char timestampSeparatorChar{ ':' };
    }

    std::string idToString(db::ImageId id)
    {
        return "im-" + id.toString();
    }

    std::string idToString(CoverArtId coverId)
    {
        // produce "id:timestamp"
        std::string res{ std::visit([](auto&& id) {
            return idToString(id);
        },
            coverId.id) };

        res += timestampSeparatorChar;
        res += std::to_string(coverId.timestamp);

        return res;
    }
} // namespace lms::api::subsonic

// Used to parse parameters
namespace lms::core::stringUtils
{
    template<>
    std::optional<db::ImageId> readAs(std::string_view str)
    {
        std::vector<std::string_view> values{ core::stringUtils::splitString(str, '-') };
        if (values.size() != 2)
            return std::nullopt;

        if (values[0] != "im")
            return std::nullopt;

        if (const auto value{ core::stringUtils::readAs<db::ReleaseId::ValueType>(values[1]) })
            return db::ImageId{ *value };

        return std::nullopt;
    }

    template<>
    std::optional<api::subsonic::CoverArtId> readAs(std::string_view str)
    {
        // expect "id:timestamp"
        auto timeStampSeparator{ str.find_last_of(api::subsonic::timestampSeparatorChar) };
        if (timeStampSeparator == std::string_view::npos)
            return std::nullopt;

        std::string_view strId{ str.substr(0, timeStampSeparator) };
        std::string_view strTimestamp{ str.substr(timeStampSeparator + 1) };

        api::subsonic::CoverArtId cover;
        if (const auto imagetId{ readAs<db::ImageId>(strId) })
            cover.id = *imagetId;
        else if (const auto trackId{ readAs<db::TrackId>(strId) })
            cover.id = *trackId;
        else
            return std::nullopt;

        if (const auto timestamp{ readAs<std::time_t>(strTimestamp) })
            cover.timestamp = *timestamp;
        else
            return std::nullopt;

        return cover;
    }
} // namespace lms::core::stringUtils
