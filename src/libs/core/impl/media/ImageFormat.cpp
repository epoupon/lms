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

#include "core/media/ImageFormat.hpp"

namespace lms::core::media
{
    core::LiteralString imageFormatToString(ImageFormat format)
    {
        switch (format)
        {
        case ImageFormat::BMP:
            return "BMP";
        case ImageFormat::GIF:
            return "GIF";
        case ImageFormat::JPEG:
            return "JPEG";
        case ImageFormat::PNG:
            return "PNG";
        case ImageFormat::SVG:
            return "SVG";
        case ImageFormat::WebP:
            return "WebP";
        }

        return "Unknown";
    }
} // namespace lms::core::media
