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

#include "FormatDesc.hpp"

#include <algorithm>
#include <array>

namespace lms::image
{
    namespace
    {
        const std::array bmpExtensions{ std::filesystem::path{ ".bmp" } };
        const std::array gifExtensions{ std::filesystem::path{ ".gif" } };
        const std::array jpegExtensions{ std::filesystem::path{ ".jpg" }, std::filesystem::path{ ".jpeg" } };
        const std::array pngExtensions{ std::filesystem::path{ ".png" } };
        const std::array webpExtensions{ std::filesystem::path{ ".webp" } };

        const std::array formatPolicies{
            FormatDesc{ .format = core::media::ImageFormat::BMP, .canDecode = true, .fileExtensions = bmpExtensions },
            FormatDesc{ .format = core::media::ImageFormat::GIF, .canDecode = false, .fileExtensions = gifExtensions },
            FormatDesc{ .format = core::media::ImageFormat::JPEG, .canDecode = true, .fileExtensions = jpegExtensions },
            FormatDesc{ .format = core::media::ImageFormat::PNG, .canDecode = true, .fileExtensions = pngExtensions },
            FormatDesc{ .format = core::media::ImageFormat::WebP, .canDecode = false, .fileExtensions = webpExtensions },
        };
    } // namespace

    void visitFormatDescs(const std::function<void(const FormatDesc&)>& visitor)
    {
        std::for_each(std::cbegin(formatPolicies), std::cend(formatPolicies), visitor);
    }

    const FormatDesc* findFormatDesc(core::media::ImageFormat format)
    {
        const auto it{ std::find_if(formatPolicies.begin(), formatPolicies.end(), [format](const auto& policy) { return policy.format == format; }) };
        if (it == formatPolicies.end())
            return nullptr;

        return &*it;
    }
} // namespace lms::image
