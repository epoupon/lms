/*
 * Copyright (C) 2024 Emeric Poupon
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
#include <optional>

#include <Wt/WDateTime.h>

#include "database/Object.hpp"

namespace lms::db
{
    class Directory;
    class MediaLibrary;
    class Session;
} // namespace lms::db

namespace lms::scanner
{
    struct FileInfo
    {
        Wt::WDateTime lastWriteTime;
        std::filesystem::path relativePath;
        std::size_t fileSize{};
    };

    namespace utils
    {
        Wt::WDateTime retrieveFileGetLastWrite(const std::filesystem::path& file);
        std::optional<FileInfo> retrieveFileInfo(const std::filesystem::path& file, const std::filesystem::path& rootPath);

        db::ObjectPtr<db::Directory> getOrCreateDirectory(db::Session& session, const std::filesystem::path& path, const db::ObjectPtr<db::MediaLibrary>& mediaLibrary);
    } // namespace utils
} // namespace lms::scanner