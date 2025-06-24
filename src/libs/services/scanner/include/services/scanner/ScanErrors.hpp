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

#include <filesystem>
#include <system_error>

namespace lms::scanner
{
    // Forward declarations of all error types
    struct ScanError;
    struct IOScanError;
    struct AudioFileScanError;
    struct EmbeddedImageScanError;
    struct NoAudioTrackFoundError;
    struct BadAudioDurationError;
    struct ArtistInfoFileScanError;
    struct MissingArtistNameError;
    struct ImageFileScanError;
    struct LyricsFileScanError;
    struct PlayListFileScanError;
    struct PlayListFilePathMissingError;
    struct PlayListFileAllPathesMissingError;

    // Visitor interface
    struct ScanErrorVisitor
    {
        virtual ~ScanErrorVisitor() = default;

        virtual void visit(const ScanError&) = 0;
        virtual void visit(const IOScanError&) = 0;
        virtual void visit(const AudioFileScanError&) = 0;
        virtual void visit(const EmbeddedImageScanError&) = 0;
        virtual void visit(const NoAudioTrackFoundError&) = 0;
        virtual void visit(const BadAudioDurationError&) = 0;
        virtual void visit(const ArtistInfoFileScanError&) = 0;
        virtual void visit(const MissingArtistNameError&) = 0;
        virtual void visit(const ImageFileScanError&) = 0;
        virtual void visit(const LyricsFileScanError&) = 0;
        virtual void visit(const PlayListFileScanError&) = 0;
        virtual void visit(const scanner::PlayListFilePathMissingError& error) = 0;
        virtual void visit(const scanner::PlayListFileAllPathesMissingError& error) = 0;
    };

    struct ScanError
    {
        ScanError(const std::filesystem::path& p)
            : path{ p } {}
        virtual ~ScanError() = default;
        virtual void accept(ScanErrorVisitor&) const = 0;

        std::filesystem::path path; // Error that occurs on this path
    };

    struct IOScanError : public ScanError
    {
        IOScanError(const std::filesystem::path& p, std::error_code e)
            : ScanError{ p }
            , err{ e } {}

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }

        std::error_code err;
    };

    struct AudioFileScanError : public ScanError
    {
        using ScanError::ScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };

    struct EmbeddedImageScanError : public AudioFileScanError
    {
        EmbeddedImageScanError(const std::filesystem::path& p, unsigned i)
            : AudioFileScanError{ p }
            , index{ i } {}

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }

        unsigned index;
    };

    struct NoAudioTrackFoundError : public AudioFileScanError
    {
        using AudioFileScanError::AudioFileScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };

    struct BadAudioDurationError : public AudioFileScanError
    {
        using AudioFileScanError::AudioFileScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };

    struct ArtistInfoFileScanError : public ScanError
    {
        using ScanError::ScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };

    struct MissingArtistNameError : public ArtistInfoFileScanError
    {
        using ArtistInfoFileScanError::ArtistInfoFileScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };

    struct ImageFileScanError : public ScanError
    {
        using ScanError::ScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };

    struct LyricsFileScanError : public ScanError
    {
        using ScanError::ScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };

    struct PlayListFileScanError : public ScanError
    {
        using ScanError::ScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };

    struct PlayListFilePathMissingError : public PlayListFileScanError
    {
        PlayListFilePathMissingError(const std::filesystem::path& p, const std::filesystem::path& e)
            : PlayListFileScanError{ p }
            , entry{ e }
        {
        }

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }

        std::filesystem::path entry;
    };

    struct PlayListFileAllPathesMissingError : public PlayListFileScanError
    {
        using PlayListFileScanError::PlayListFileScanError;

        void accept(ScanErrorVisitor& visitor) const override
        {
            visitor.visit(*this);
        }
    };
} // namespace lms::scanner
