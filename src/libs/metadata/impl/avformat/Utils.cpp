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

#include "Utils.hpp"

namespace lms::metadata::avformat::utils
{
    std::span<const std::filesystem::path> getSupportedExtensions()
    {
        // TODO: use av capability to retrieve supported formats
        static const std::array<std::filesystem::path, 18> fileExtensions{
            ".aac",
            ".alac",
            ".aif",
            ".aiff",
            ".ape",
            ".dsf",
            ".flac",
            ".m4a",
            ".m4b",
            ".mp3",
            ".mpc",
            ".oga",
            ".ogg",
            ".opus",
            ".shn",
            ".wav",
            ".wma",
            ".wv",
        };
        return fileExtensions;
    }
} // namespace lms::metadata::avformat::utils