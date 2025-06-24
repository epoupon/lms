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

#include <taglib/audioproperties.h>
#include <taglib/tfile.h>

#include "core/TaggedType.hpp"
#include "metadata/Types.hpp"

namespace lms::metadata::taglib::utils
{
    std::span<const std::filesystem::path> getSupportedExtensions();
    TagLib::AudioProperties::ReadStyle readStyleToTagLibReadStyle(ParserReadStyle readStyle);

    using ReadAudioProperties = core::TaggedBool<struct ReadAudioPropertiesTag>;
    std::unique_ptr<TagLib::File> parseFile(const std::filesystem::path& p, TagLib::AudioProperties::ReadStyle readStyle, ReadAudioProperties readAudioProperties);
} // namespace lms::metadata::taglib::utils