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

#pragma once

#include <filesystem>
#include <vector>

#include "image/IEncodedImage.hpp"

namespace lms::image
{
    class EncodedImage : public IEncodedImage
    {
    public:
        EncodedImage(const std::filesystem::path& path);
        EncodedImage(std::vector<std::byte>&& data, std::string_view mimeType);
        EncodedImage(std::span<const std::byte> data, std::string_view mimeType);
        ~EncodedImage() override = default;
        EncodedImage(const EncodedImage&) = delete;
        EncodedImage& operator=(const EncodedImage&) = delete;

        std::span<const std::byte> getData() const override { return _data; }
        std::string_view getMimeType() const override { return _mimeType; }

    private:
        const std::vector<std::byte> _data;
        const std::string _mimeType;
    };
} // namespace lms::image