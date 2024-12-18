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

#include <tuple>
#include <unordered_map>

#include "core/String.hpp"

#include "ReleaseTypes.hpp"

namespace lms::core::stringUtils
{
    template<>
    std::optional<ui::PrimaryReleaseType> readAs(std::string_view str)
    {
        static const std::unordered_map<std::string, ui::PrimaryReleaseType> entries{
            { "album", ui::PrimaryReleaseType::Album },
            { "single", ui::PrimaryReleaseType::Single },
            { "ep", ui::PrimaryReleaseType::EP },
            { "broadcast", ui::PrimaryReleaseType::Broadcast },
            { "other", ui::PrimaryReleaseType::Other },
        };

        const auto it{ entries.find(stringToLower(stringTrim(str))) };
        if (it == std::cend(entries))
            return std::nullopt;

        return it->second;
    }

    template<>
    std::optional<ui::SecondaryReleaseType> readAs(std::string_view str)
    {
        static const std::unordered_map<std::string, ui::SecondaryReleaseType> entries{
            { "compilation", ui::SecondaryReleaseType::Compilation },
            { "soundtrack", ui::SecondaryReleaseType::Soundtrack },
            { "spokenword", ui::SecondaryReleaseType::Spokenword },
            { "interview", ui::SecondaryReleaseType::Interview },
            { "audiobook", ui::SecondaryReleaseType::Audiobook },
            { "audio drama", ui::SecondaryReleaseType::AudioDrama },
            { "live", ui::SecondaryReleaseType::Live },
            { "remix", ui::SecondaryReleaseType::Remix },
            { "dj-mix", ui::SecondaryReleaseType::DJMix },
            { "mixtape/street", ui::SecondaryReleaseType::Mixtape_Street },
            { "demo", ui::SecondaryReleaseType::Demo },
            { "field recording", ui::SecondaryReleaseType::FieldRecording },
        };

        const auto it{ entries.find(stringToLower(stringTrim(str))) };
        if (it == std::cend(entries))
            return std::nullopt;

        return it->second;
    }
} // namespace lms::core::stringUtils

namespace lms::ui
{
    ReleaseType parseReleaseType(const std::vector<std::string>& releaseTypeNames)
    {
        ReleaseType res;

        for (std::string_view releaseTypeName : releaseTypeNames)
        {
            if (auto primaryType{ core::stringUtils::readAs<PrimaryReleaseType>(releaseTypeName) })
            {
                if (!res.primaryType)
                    res.primaryType = primaryType;
                else
                    res.customTypes.push_back(std::string{ releaseTypeName });
            }
            else if (auto secondaryType{ core::stringUtils::readAs<SecondaryReleaseType>(releaseTypeName) })
            {
                res.secondaryTypes.insert(*secondaryType);
            }
            else
                res.customTypes.push_back(std::string{ releaseTypeName });
        }

        return res;
    }

    bool operator<(std::optional<PrimaryReleaseType> typeA, std::optional<PrimaryReleaseType> typeB)
    {
        if (!typeA && typeB)
            return false;
        else if (typeA && !typeB)
            return true;
        else
            return static_cast<int>(*typeA) < static_cast<int>(*typeB);
    }

    bool operator<(core::EnumSet<SecondaryReleaseType> typesA, core::EnumSet<SecondaryReleaseType> typesB)
    {
        return typesA.getBitfield() < typesB.getBitfield();
    }

    bool ReleaseType::operator<(const ReleaseType& other) const
    {
        // TODO : order custom types and compare for each element (size is not to be compared first)
        return std::tie(primaryType, secondaryTypes, customTypes) < std::tie(other.primaryType, other.secondaryTypes, other.customTypes);
    }
} // namespace lms::ui
