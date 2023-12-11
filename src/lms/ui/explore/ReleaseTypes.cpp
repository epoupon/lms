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

#include "ReleaseTypes.hpp"
#include "utils/String.hpp"

namespace StringUtils
{
    template<>
    std::optional<UserInterface::PrimaryReleaseType> readAs(std::string_view str)
    {
        static const std::unordered_map<std::string, UserInterface::PrimaryReleaseType> entries
        {
            {"album", UserInterface::PrimaryReleaseType::Album},
            {"single", UserInterface::PrimaryReleaseType::Single},
            {"ep", UserInterface::PrimaryReleaseType::EP},
            {"broadcast", UserInterface::PrimaryReleaseType::Broadcast},
            {"other", UserInterface::PrimaryReleaseType::Other},
        };

        const auto it{ entries.find(stringToLower(stringTrim(str))) };
        if (it == std::cend(entries))
            return std::nullopt;

        return it->second;
    }

    template<>
    std::optional<UserInterface::SecondaryReleaseType> readAs(std::string_view str)
    {
        static const std::unordered_map<std::string, UserInterface::SecondaryReleaseType> entries
        {
            {"compilation", UserInterface::SecondaryReleaseType::Compilation},
            {"soundtrack", UserInterface::SecondaryReleaseType::Soundtrack},
            {"spokenword", UserInterface::SecondaryReleaseType::Spokenword},
            {"interview", UserInterface::SecondaryReleaseType::Interview},
            {"audiobook", UserInterface::SecondaryReleaseType::Audiobook},
            {"audio drama", UserInterface::SecondaryReleaseType::AudioDrama},
            {"live", UserInterface::SecondaryReleaseType::Live},
            {"remix", UserInterface::SecondaryReleaseType::Remix},
            {"dj-mix", UserInterface::SecondaryReleaseType::DJMix},
            {"mixtape/street", UserInterface::SecondaryReleaseType::Mixtape_Street},
            {"demo", UserInterface::SecondaryReleaseType::Demo},
            {"field recording", UserInterface::SecondaryReleaseType::FieldRecording},
        };

        const auto it{ entries.find(stringToLower(stringTrim(str))) };
        if (it == std::cend(entries))
            return std::nullopt;

        return it->second;
    }
}

namespace UserInterface
{
    ReleaseType parseReleaseType(const std::vector<std::string>& releaseTypeNames)
    {
        ReleaseType res;

        for (std::string_view releaseTypeName : releaseTypeNames)
        {
            if (auto primaryType{ StringUtils::readAs<PrimaryReleaseType>(releaseTypeName) })
            {
                if (!res.primaryType)
                    res.primaryType = primaryType;
                else
                    res.customTypes.push_back(std::string{ releaseTypeName });
            }
            else if (auto secondaryType{ StringUtils::readAs<SecondaryReleaseType>(releaseTypeName) })
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
            return  static_cast<int>(*typeA) < static_cast<int>(*typeB);
    }

    bool operator<(EnumSet<SecondaryReleaseType> typesA, EnumSet<SecondaryReleaseType> typesB)
    {
        return typesA.getBitfield() < typesB.getBitfield();
    }

    bool ReleaseType::operator<(const ReleaseType& other) const
    {
        // TODO : order custom types and compare for each element (size is not to be compared first)
        return std::tie(primaryType, secondaryTypes, customTypes) < std::tie(other.primaryType, other.secondaryTypes, other.customTypes);
    }
} // namespace UserInterface
