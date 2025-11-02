/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "audio/IImageReader.hpp"

namespace lms::audio
{
    core::LiteralString imageTypeToString(Image::Type type)
    {
        switch (type)
        {
        case Image::Type::Other:
            return "Other";
        case Image::Type::FileIcon:
            return "FileIcon";
        case Image::Type::OtherFileIcon:
            return "OtherFileIcon";
        case Image::Type::FrontCover:
            return "FrontCover";
        case Image::Type::BackCover:
            return "BackCover";
        case Image::Type::LeafletPage:
            return "LeafletPage";
        case Image::Type::Media:
            return "Media";
        case Image::Type::LeadArtist:
            return "LeadArtist";
        case Image::Type::Artist:
            return "Artist";
        case Image::Type::Conductor:
            return "Conductor";
        case Image::Type::Band:
            return "Band";
        case Image::Type::Composer:
            return "Composer";
        case Image::Type::Lyricist:
            return "Lyricist";
        case Image::Type::RecordingLocation:
            return "RecordingLocation";
        case Image::Type::DuringRecording:
            return "DuringRecording";
        case Image::Type::DuringPerformance:
            return "DuringPerformance";
        case Image::Type::MovieScreenCapture:
            return "MovieScreenCapture";
        case Image::Type::ColouredFish:
            return "ColouredFish";
        case Image::Type::Illustration:
            return "Illustration";
        case Image::Type::BandLogo:
            return "BandLogo";
        case Image::Type::PublisherLogo:
            return "PublisherLogo";
        case Image::Type::Unknown:
            break;
        }

        return "Unknown";
    }

} // namespace lms::audio