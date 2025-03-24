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

#include "core/String.hpp"
#include "database/ImageId.hpp"
#include "database/TrackEmbeddedImageId.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        constexpr char timestampSeparatorChar{ '-' };

        std::string idToString(db::ImageId id)
        {
            return "im-" + id.toString();
        }

        std::string idToString(db::TrackEmbeddedImageId id)
        {
            return "trim-" + id.toString();
        }
    } // namespace

    std::string idToString(CoverArtId coverId)
    {
        std::string res;

        // produce "type-id[-timestamp]"
        if (db::TrackEmbeddedImageId * imageId{ std::get_if<db::TrackEmbeddedImageId>(&coverId.id) })
        {
            res = idToString(*imageId);
            assert(!coverId.timestamp.has_value());
        }
        else if (db::ImageId * imageId{ std::get_if<db::ImageId>(&coverId.id) })
        {
            res = idToString(*imageId);
            res += timestampSeparatorChar;
            assert(coverId.timestamp.has_value());
            res += std::to_string(*coverId.timestamp);
        }

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
        if (values.size() <= 1)
            return std::nullopt;

        if (values[0] == "trim")
        {
            // expect "trim-id"
            if (values.size() == 2)
            {
                if (const auto value{ core::stringUtils::readAs<db::TrackEmbeddedImageId::ValueType>(values[1]) })
                    res.emplace(db::TrackEmbeddedImageId{ *value });
            }
        }
        else if (values[0] == "im")
        {
            // expect "im-id-timestamp"
            if (values.size() == 3)
            {
                const auto imageId{ core::stringUtils::readAs<db::ImageId::ValueType>(values[1]) };
                const auto timestamp{ core::stringUtils::readAs<std::time_t>(values[2]) };

                if (imageId && timestamp)
                    res.emplace(db::ImageId{ *imageId }, *timestamp);
            }
        }

        return res;
    }
} // namespace lms::core::stringUtils
