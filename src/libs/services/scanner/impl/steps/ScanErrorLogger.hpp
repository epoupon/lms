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
    class ScanErrorLogger : public ScanErrorVisitor
    {
    private:
        void visit(const ScanError& error) override;
        void visit(const IOScanError& error) override;
        void visit(const AudioFileScanError& error) override;
        void visit(const EmbeddedImageScanError& error) override;
        void visit(const NoAudioTrackFoundError& error) override;
        void visit(const BadAudioDurationError& error) override;
        void visit(const ArtistInfoFileScanError& error) override;
        void visit(const MissingArtistNameError& error) override;
        void visit(const ImageFileScanError& error) override;
        void visit(const LyricsFileScanError& error) override;
        void visit(const PlayListFileScanError& error) override;
        void visit(const PlayListFilePathMissingError& error) override;
        void visit(const PlayListFileAllPathesMissingError& error) override;
        void visit(const MusicNNEmbeddingsExtractError& error) override;
    };
} // namespace lms::scanner