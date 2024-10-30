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

#include "database/MediaLibraryId.hpp"
#include "database/ScanSettings.hpp"

namespace lms::scanner
{
    struct ScannerSettings
    {
        std::size_t scanVersion{};
        Wt::WTime startTime;
        db::ScanSettings::UpdatePeriod updatePeriod{ db::ScanSettings::UpdatePeriod::Never };
        std::vector<std::filesystem::path> supportedAudioFileExtensions;
        std::vector<std::filesystem::path> supportedImageFileExtensions;
        std::vector<std::filesystem::path> supportedLyricsFileExtensions;
        bool skipDuplicateMBID{};
        std::vector<std::string> extraTags;
        std::vector<std::string> artistTagDelimiters;
        std::vector<std::string> defaultTagDelimiters;

        struct MediaLibraryInfo
        {
            db::MediaLibraryId id;
            std::filesystem::path rootDirectory;

            auto operator<=>(const MediaLibraryInfo& other) const = default;
        };
        std::vector<MediaLibraryInfo> mediaLibraries;

        bool operator==(const ScannerSettings& rhs) const = default;
    };
} // namespace lms::scanner
