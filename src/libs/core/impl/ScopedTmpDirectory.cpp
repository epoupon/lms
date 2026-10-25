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

#include "core/ScopedTmpDirectory.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace lms::core
{
    ScopedTmpDirectory::ScopedTmpDirectory(std::string_view prefix)
    {
        std::string pathTemplate{ (std::filesystem::temp_directory_path() / (std::string{ prefix } + "XXXXXX")).string() };
        if (!::mkdtemp(pathTemplate.data()))
            throw std::runtime_error{ "Cannot create temporary directory from " + pathTemplate };

        _path = pathTemplate;
    }

    ScopedTmpDirectory::~ScopedTmpDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }

    std::filesystem::path ScopedTmpDirectory::operator/(std::string_view fileName) const
    {
        return _path / fileName;
    }
} // namespace lms::core
