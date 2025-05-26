/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "ScanErrorLogger.hpp"

#include <cassert>

#include "core/ILogger.hpp"

namespace lms::scanner
{
    void ScanErrorLogger::visit([[maybe_unused]] const scanner::ScanError& error)
    {
        // There should never be a ScanError without a specific type
        assert(false);
    }

    void ScanErrorLogger::visit(const scanner::IOScanError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to open file " << error.path << ": " << error.err.message());
    }

    void ScanErrorLogger::visit(const scanner::AudioFileScanError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to parse audio file " << error.path);
    }

    void ScanErrorLogger::visit(const scanner::EmbeddedImageScanError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to parse image in track file " << error.path << " at index " << error.index);
    }

    void ScanErrorLogger::visit(const scanner::NoAudioTrackFoundError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to parse audio file " << error.path << ": no audio track found");
    }

    void ScanErrorLogger::visit(const scanner::BadAudioDurationError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to parse audio file " << error.path << ": duration is 0");
    }

    void ScanErrorLogger::visit(const scanner::ArtistInfoFileScanError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to read artist info file " << error.path);
    }

    void ScanErrorLogger::visit(const scanner::MissingArtistNameError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to read artist info file " << error.path << ": missing name");
    }

    void ScanErrorLogger::visit(const scanner::ImageFileScanError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to read image file " << error.path);
    }

    void ScanErrorLogger::visit(const scanner::LyricsFileScanError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to read lyrics file " << error.path);
    }

    void ScanErrorLogger::visit(const scanner::PlayListFileScanError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to read playlist file " << error.path);
    }

    void ScanErrorLogger::visit(const scanner::PlayListFilePathMissingError& error)
    {
        LMS_LOG(DBUPDATER, DEBUG, "Track " << error.entry << " not found in playlist " << error.path);
    }

    void ScanErrorLogger::visit(const scanner::PlayListFileAllPathesMissingError& error)
    {
        LMS_LOG(DBUPDATER, ERROR, "Failed to parse playlist " << error.path << ": all entries are missing");
    }
} // namespace lms::scanner