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
#include <memory>
#include <span>

#include <taglib/tfile.h>
#include <taglib/tfilestream.h>

#include "audio/IAudioFileInfoParser.hpp"

namespace lms::audio::taglib::utils
{
    void init();

    std::span<const std::filesystem::path> getSupportedExtensions();

    struct FileDesc
    {
        std::unique_ptr<::TagLib::FileStream> fileStream;
        std::unique_ptr<::TagLib::File> file;
    };
    FileDesc parseFile(const std::filesystem::path& p, AudioFileInfoParseOptions::AudioPropertiesReadStyle readStyle);
} // namespace lms::audio::taglib::utils