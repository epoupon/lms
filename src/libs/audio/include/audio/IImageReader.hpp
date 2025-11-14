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

#include <functional>
#include <span>
#include <string>

#include "core/LiteralString.hpp"

namespace lms::audio
{
    struct Image
    {
        // See TagLib types (based on ID3v2 APIC types)
        enum class Type
        {
            // No information
            Unknown,
            // A type not enumerated below
            Other,
            // 32x32 PNG image that should be used as the file icon
            FileIcon,
            // File icon of a different size or format
            OtherFileIcon,
            // Front cover image of the album
            FrontCover,
            // Back cover image of the album
            BackCover,
            // Inside leaflet page of the album
            LeafletPage,
            // Image from the album itself
            Media,
            // Picture of the lead artist or soloist
            LeadArtist,
            // Picture of the artist or performer
            Artist,
            // Picture of the conductor
            Conductor,
            // Picture of the band or orchestra
            Band,
            // Picture of the composer
            Composer,
            // Picture of the lyricist or text writer
            Lyricist,
            // Picture of the recording location or studio
            RecordingLocation,
            // Picture of the artists during recording
            DuringRecording,
            // Picture of the artists during performance
            DuringPerformance,
            // Picture from a movie or video related to the track
            MovieScreenCapture,
            // Picture of a large, coloured fish
            ColouredFish,
            // Illustration related to the track
            Illustration,
            // Logo of the band or performer
            BandLogo,
            // Logo of the publisher (record company)
            PublisherLogo
        };

        Type type{ Type::Unknown };
        std::string mimeType{ "application/octet-stream" };
        std::string description;
        std::span<const std::byte> data;
    };

    core::LiteralString imageTypeToString(Image::Type type);

    class IImageReader
    {
    public:
        virtual ~IImageReader() = default;

        using ImageVisitor = std::function<void(const Image& image)>;
        virtual void visitImages(const ImageVisitor& visitor) const = 0;
    };
} // namespace lms::audio
