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

#include "audio/IImageReader.hpp"

namespace TagLib
{
    class File;
}

namespace lms::audio::taglib
{
    class ImageReader : public IImageReader
    {
    public:
        ImageReader(TagLib::File& _file);
        ~ImageReader() override;
        ImageReader(const ImageReader&) = delete;
        ImageReader& operator=(const ImageReader&) = delete;

    private:
        void visitImages(const ImageVisitor& visitor) const override;

        TagLib::File& _file;
    };
} // namespace lms::audio::taglib
