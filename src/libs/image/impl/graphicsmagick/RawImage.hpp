/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <cstddef>
#include <filesystem>
#include <span>

#include <Magick++.h>

#include "image/IRawImage.hpp"

namespace lms::image::GraphicsMagick
{
    class RawImage : public IRawImage
    {
    public:
        RawImage(std::span<const std::byte> encodedData);
        RawImage(const std::filesystem::path& path);

        ImageSize getWidth() const override;
        ImageSize getHeight() const override;

        void resize(ImageSize width) override;

        Magick::Image getMagickImage() const;

    private:
        Magick::Image _image;
    };
} // namespace lms::image::GraphicsMagick
