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

#include "SubsonicId.hpp"

#include "SubsonicResponse.hpp"

#include "core/ILogger.hpp"
#include "core/String.hpp"

namespace lms::api::subsonic
{
    std::string idToString(db::ArtistId id)
    {
        return "ar-" + id.toString();
    }

    std::string idToString(db::DirectoryId id)
    {
        return "dir-" + id.toString();
    }

    std::string idToString(db::MediaLibraryId id)
    {
        // No need to prefix as this is only used at well known places
        return id.toString();
    }

    std::string idToString(db::ReleaseId id)
    {
        return "al-" + id.toString();
    }

    std::string idToString(RootId)
    {
        return "root";
    }

    std::string idToString(db::TrackId id)
    {
        return "tr-" + id.toString();
    }

    std::string idToString(db::TrackListId id)
    {
        return "pl-" + id.toString();
    }
} // namespace lms::api::subsonic

namespace lms::core::stringUtils
{
    template<>
    std::optional<db::ArtistId> readAs(std::string_view str)
    {
        std::vector<std::string_view> values{ core::stringUtils::splitString(str, '-') };
        if (values.size() != 2)
            return std::nullopt;

        if (values[0] != "ar")
            return std::nullopt;

        if (const auto value{ core::stringUtils::readAs<db::ArtistId::ValueType>(values[1]) })
            return db::ArtistId{ *value };

        return std::nullopt;
    }

    template<>
    std::optional<db::DirectoryId> readAs(std::string_view str)
    {
        std::vector<std::string_view> values{ core::stringUtils::splitString(str, '-') };
        if (values.size() != 2)
            return std::nullopt;

        if (values[0] != "dir")
            return std::nullopt;

        if (const auto value{ core::stringUtils::readAs<db::DirectoryId::ValueType>(values[1]) })
            return db::DirectoryId{ *value };

        return std::nullopt;
    }

    template<>
    std::optional<db::MediaLibraryId> readAs(std::string_view str)
    {
        if (const auto value{ core::stringUtils::readAs<db::MediaLibraryId::ValueType>(str) })
            return db::MediaLibraryId{ *value };

        return std::nullopt;
    }

    template<>
    std::optional<db::ReleaseId> readAs(std::string_view str)
    {
        std::vector<std::string_view> values{ core::stringUtils::splitString(str, '-') };
        if (values.size() != 2)
            return std::nullopt;

        if (values[0] != "al")
            return std::nullopt;

        if (const auto value{ core::stringUtils::readAs<db::ReleaseId::ValueType>(values[1]) })
            return db::ReleaseId{ *value };

        return std::nullopt;
    }

    template<>
    std::optional<api::subsonic::RootId> readAs(std::string_view str)
    {
        if (str == "root")
            return api::subsonic::RootId{};

        return std::nullopt;
    }

    template<>
    std::optional<db::TrackId> readAs(std::string_view str)
    {
        std::vector<std::string_view> values{ core::stringUtils::splitString(str, '-') };
        if (values.size() != 2)
            return std::nullopt;

        if (values[0] != "tr")
            return std::nullopt;

        if (const auto value{ core::stringUtils::readAs<db::TrackId::ValueType>(values[1]) })
            return db::TrackId{ *value };

        return std::nullopt;
    }

    template<>
    std::optional<db::TrackListId> readAs(std::string_view str)
    {
        std::vector<std::string_view> values{ core::stringUtils::splitString(str, '-') };
        if (values.size() != 2)
            return std::nullopt;

        if (values[0] != "pl")
            return std::nullopt;

        if (const auto value{ core::stringUtils::readAs<db::TrackListId::ValueType>(values[1]) })
            return db::TrackListId{ *value };

        return std::nullopt;
    }
} // namespace lms::core::stringUtils
