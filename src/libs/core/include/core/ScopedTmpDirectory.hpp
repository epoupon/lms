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
#include <string_view>

namespace lms::core
{
    // A tmp directory removed on destruction
    class ScopedTmpDirectory
    {
    public:
        // The directory name is the prefix followed by a unique suffix
        explicit ScopedTmpDirectory(std::string_view prefix = "lms-");
        ~ScopedTmpDirectory();

        ScopedTmpDirectory(const ScopedTmpDirectory&) = delete;
        ScopedTmpDirectory& operator=(const ScopedTmpDirectory&) = delete;

        const std::filesystem::path& getPath() const { return _path; }
        std::filesystem::path operator/(std::string_view fileName) const;

    private:
        std::filesystem::path _path;
    };
} // namespace lms::core
