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

#include "core/Utils.hpp"
#include "core/media/ImageType.hpp"

namespace lms::audio
{
    struct Image
    {
        core::media::ImageType type{ core::media::ImageType::Unknown };
        std::string mimeType{ "application/octet-stream" };
        std::string description;
        std::span<const std::byte> data;
    };

    class IImageReader
    {
    public:
        virtual ~IImageReader() = default;

        using ImageVisitor = std::function<core::VisitorResult(const Image& image)>;
        virtual void visitImages(const ImageVisitor& visitor) const = 0;
    };
} // namespace lms::audio
