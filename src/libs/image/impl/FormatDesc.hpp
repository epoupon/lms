/*
 * Copyright (C) 2026 Emeric Poupon
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
#include <functional>
#include <span>

#include "core/media/ImageFormat.hpp"

namespace lms::image
{
    struct FormatDesc
    {
        core::media::ImageFormat format;
        bool canDecode; // if false, no codec path: never decoded nor resized, bytes served as-is
        std::span<const std::filesystem::path> fileExtensions;
    };

    void visitFormatDescs(const std::function<void(const FormatDesc&)>& visitor);
    const FormatDesc* findFormatDesc(core::media::ImageFormat format); // nullptr if not scannable
} // namespace lms::image
