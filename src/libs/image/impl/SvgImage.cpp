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

#include "SvgImage.hpp"

#include <fstream>

#include "core/ITraceLogger.hpp"
#include "image/Exception.hpp"

namespace lms::image
{
    std::unique_ptr<IEncodedImage> readSvgFile(const std::filesystem::path& p)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "ReadSVG");

        if (p.extension() != ".svg")
            throw Exception{ "Unexpected file extension: '" + p.extension().string() + "', expected .svg" };

        std::ifstream ifs{ p.string(), std::ios::binary };
        if (!ifs.is_open())
            throw Exception{ "Cannot open file '" + p.string() + "' for reading purpose" };

        std::vector<std::byte> data;
        // read file content
        ifs.seekg(0, std::ios::end);
        std::streamsize size = ifs.tellg();
        if (size < 0)
            throw Exception{ "Cannot determine file size for '" + p.string() + "'" };

        ifs.seekg(0, std::ios::beg);
        data.resize(size);
        if (!ifs.read(reinterpret_cast<char*>(data.data()), size))
            throw Exception{ "Cannot read file content for '" + p.string() + "'" };

        return std::make_unique<SvgImage>(std::move(data));
    }
} // namespace lms::image