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

namespace lms::db::detail
{
    // For enums that have explicit values, never change enum values as they are stored in database.
    // => only add new values or remove unused values, do not recycle values.
    enum class ImageType
    {
        Unknown = 0,
        Other = 1,
        FileIcon = 2,
        OtherFileIcon = 3,
        FrontCover = 4,
        BackCover = 5,
        LeafletPage = 6,
        Media = 7,
        LeadArtist = 8,
        Artist = 9,
        Conductor = 10,
        Band = 11,
        Composer = 12,
        Lyricist = 13,
        RecordingLocation = 14,
        DuringRecording = 15,
        DuringPerformance = 16,
        MovieScreenCapture = 17,
        ColouredFish = 18,
        Illustration = 19,
        BandLogo = 20,
        PublisherLogo = 21
    };

    enum class Container
    {
        Unknown = 0,

        AIFF = 1,
        APE = 2,
        ASF = 3,
        DSF = 4,
        FLAC = 5,
        MP4 = 6,
        MPC = 7,
        MPEG = 8,
        Ogg = 9,
        Shorten = 10,
        TrueAudio = 11,
        WAV = 12,
        WavPack = 13,
    };

    enum class Codec
    {
        Unknown = 0,

        AAC = 1,
        AC3 = 2,
        ALAC = 3,
        APE = 4,
        DSD = 5,
        EAC3 = 6,
        FLAC = 7,
        MP3 = 8,
        MP4ALS = 9,
        MPC7 = 10,
        MPC8 = 11,
        Opus = 12,
        PCM = 13,
        Shorten = 14,
        TrueAudio = 15,
        Vorbis = 16,
        WavPack = 17,
        WMA1 = 18,
        WMA2 = 19,
        WMA9Pro = 20,
        WMA9Lossless = 21,
    };

    enum class ImageFormat
    {
        Unknown = 0,

        BMP = 1,
        GIF = 2,
        JPEG = 3,
        PNG = 4,
        SVG = 5,
        WebP = 6,
    };
} // namespace lms::db::detail
