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

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/EnumSet.hpp"

namespace lms::ui
{
    // see https://musicbrainz.org/doc/Release_Group/Type
    enum class PrimaryReleaseType
    {
        Album,
        Single,
        EP,
        Broadcast,
        Other
    };

    enum class SecondaryReleaseType
    {
        Compilation,
        Soundtrack,
        Spokenword,
        Interview,
        Audiobook,
        AudioDrama,
        Live,
        Remix,
        DJMix,
        Mixtape_Street,
        Demo,
        FieldRecording,
    };

    struct ReleaseType
    {
        std::optional<PrimaryReleaseType> primaryType;
        core::EnumSet<SecondaryReleaseType> secondaryTypes;
        std::vector<std::string> customTypes;

        bool operator<(const ReleaseType& other) const;
    };

    ReleaseType parseReleaseType(const std::vector<std::string>& releaseTypeNames);

} // namespace lms::ui