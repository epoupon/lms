/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "image/IEncodedImage.hpp"
#include "image/IRawImage.hpp"

namespace lms::image
{
    void init(const std::filesystem::path& path);
    std::span<const std::filesystem::path> getSupportedFileExtensions();
    std::unique_ptr<IRawImage> decodeImage(const std::byte* encodedData, std::size_t encodedDataSize);
    std::unique_ptr<IRawImage> decodeImage(const std::filesystem::path& path);
    std::unique_ptr<IEncodedImage> readSvgFile(const std::filesystem::path& path);
} // namespace lms::image