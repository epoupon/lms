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

#pragma once

#include "services/scanner/ScanErrors.hpp"

namespace lms::scanner
{
    class ScanErrorLogger : public scanner::ScanErrorVisitor
    {
    private:
        void visit(const scanner::ScanError&) override;
        void visit(const scanner::IOScanError& error) override;
        void visit(const scanner::AudioFileScanError& error) override;
        void visit(const scanner::EmbeddedImageScanError& error) override;
        void visit(const scanner::NoAudioTrackFoundError& error) override;
        void visit(const scanner::BadAudioDurationError& error) override;
        void visit(const scanner::ArtistInfoFileScanError& error) override;
        void visit(const scanner::MissingArtistNameError& error) override;
        void visit(const scanner::ImageFileScanError& error) override;
        void visit(const scanner::LyricsFileScanError& error) override;
        void visit(const scanner::PlayListFileScanError& error) override;
        void visit(const scanner::PlayListFilePathMissingError& error) override;
        void visit(const scanner::PlayListFileAllPathesMissingError& error) override;
    };
} // namespace lms::scanner