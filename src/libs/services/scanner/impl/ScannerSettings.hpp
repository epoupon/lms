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

#include <filesystem>
#include <string>
#include <vector>

#include <Wt/WDateTime.h>

#include "database/objects/ScanSettings.hpp"

#include "MediaLibraryInfo.hpp"

namespace lms::scanner
{
    static inline const std::filesystem::path excludeDirFileName{ ".lmsignore" };

    struct ScannerSettings
    {
        std::size_t audioScanVersion{};
        std::size_t artistInfoScanVersion{};
        Wt::WTime startTime;
        db::ScanSettings::UpdatePeriod updatePeriod{ db::ScanSettings::UpdatePeriod::Never };
        bool skipDuplicateTrackMBID{};
        std::vector<std::string> extraTags;
        std::vector<std::string> artistTagDelimiters;
        std::vector<std::string> artistsToNotSplit;
        std::vector<std::string> defaultTagDelimiters;
        bool skipSingleReleasePlayLists{};
        bool allowArtistMBIDFallback{ true };
        bool artistImageFallbackToRelease{};

        std::vector<MediaLibraryInfo> mediaLibraries;

        bool operator==(const ScannerSettings& rhs) const = default;
    };
} // namespace lms::scanner
