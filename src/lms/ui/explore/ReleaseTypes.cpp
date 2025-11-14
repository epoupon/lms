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

#include "ReleaseTypes.hpp"

#include <tuple>
#include <unordered_map>

#include "core/String.hpp"

namespace lms::core::stringUtils
{
    template<>
    std::optional<ui::PicardReleaseType::PrimaryType> readAs(std::string_view str)
    {
        static const std::unordered_map<std::string, ui::PicardReleaseType::PrimaryType> entries{
            { "album", ui::PicardReleaseType::PrimaryType::Album },
            { "single", ui::PicardReleaseType::PrimaryType::Single },
            { "ep", ui::PicardReleaseType::PrimaryType::EP },
            { "broadcast", ui::PicardReleaseType::PrimaryType::Broadcast },
            { "other", ui::PicardReleaseType::PrimaryType::Other },
        };

        const auto it{ entries.find(stringToLower(stringTrim(str))) };
        if (it == std::cend(entries))
            return std::nullopt;

        return it->second;
    }

    template<>
    std::optional<ui::PicardReleaseType::SecondaryType> readAs(std::string_view str)
    {
        static const std::unordered_map<std::string, ui::PicardReleaseType::SecondaryType> entries{
            { "compilation", ui::PicardReleaseType::SecondaryType::Compilation },
            { "soundtrack", ui::PicardReleaseType::SecondaryType::Soundtrack },
            { "spokenword", ui::PicardReleaseType::SecondaryType::Spokenword },
            { "interview", ui::PicardReleaseType::SecondaryType::Interview },
            { "audiobook", ui::PicardReleaseType::SecondaryType::Audiobook },
            { "audio drama", ui::PicardReleaseType::SecondaryType::AudioDrama },
            { "live", ui::PicardReleaseType::SecondaryType::Live },
            { "remix", ui::PicardReleaseType::SecondaryType::Remix },
            { "dj-mix", ui::PicardReleaseType::SecondaryType::DJMix },
            { "mixtape/street", ui::PicardReleaseType::SecondaryType::Mixtape_Street },
            { "demo", ui::PicardReleaseType::SecondaryType::Demo },
            { "field recording", ui::PicardReleaseType::SecondaryType::FieldRecording },
        };

        const auto it{ entries.find(stringToLower(stringTrim(str))) };
        if (it == std::cend(entries))
            return std::nullopt;

        return it->second;
    }
} // namespace lms::core::stringUtils

namespace lms::ui
{
    std::optional<PicardReleaseType> parsePicardReleaseType(const std::vector<std::string>& releaseTypeNames)
    {
        if (releaseTypeNames.empty())
            return std::nullopt;

        const auto primaryType{ core::stringUtils::readAs<PicardReleaseType::PrimaryType>(releaseTypeNames[0]) };
        if (!primaryType)
            return std::nullopt;

        PicardReleaseType res{ .primaryType = *primaryType, .secondaryTypes = {} };
        for (std::size_t i{ 1 }; i < releaseTypeNames.size(); ++i)
        {
            const auto secondaryType{ core::stringUtils::readAs<PicardReleaseType::SecondaryType>(releaseTypeNames[i]) };
            if (!secondaryType)
                return std::nullopt;

            res.secondaryTypes.insert(*secondaryType);
        }

        return res;
    }

    ReleaseType parseReleaseType(const std::vector<std::string>& releaseTypeNames)
    {
        if (const auto picardReleaseType{ parsePicardReleaseType(releaseTypeNames) })
            return *picardReleaseType;

        return CustomReleaseType{ .types = releaseTypeNames };
    }

    bool operator<(core::EnumSet<PicardReleaseType::SecondaryType> lhs, core::EnumSet<PicardReleaseType::SecondaryType> rhs)
    {
        return lhs.getBitfield() < rhs.getBitfield();
    }

    bool PicardReleaseType::operator<(const PicardReleaseType& other) const
    {
        return std::tie(primaryType, secondaryTypes) < std::tie(other.primaryType, other.secondaryTypes);
    }

    bool CustomReleaseType::operator<(const CustomReleaseType& other) const
    {
        return types < other.types;
    }
} // namespace lms::ui
