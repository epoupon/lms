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

#include "Utils.hpp"

#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/Directory.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Session.hpp"

namespace lms::scanner::utils
{
    Wt::WDateTime retrieveFileGetLastWrite(const std::filesystem::path& file)
    {
        Wt::WDateTime res;

        try
        {
            res = core::pathUtils::getLastWriteTime(file);
        }
        catch (core::LmsException& e)
        {
            LMS_LOG(DBUPDATER, ERROR, "Cannot get last write time: " << e.what());
        }

        return res;
    }

    std::optional<FileInfo> retrieveFileInfo(const std::filesystem::path& file, const std::filesystem::path& rootPath)
    {
        std::optional<FileInfo> res;
        res.emplace();

        res->lastWriteTime = retrieveFileGetLastWrite(file);
        if (!res->lastWriteTime.isValid())
        {
            res.reset();
            return res;
        }

        {
            std::error_code ec;
            res->relativePath = std::filesystem::relative(file, rootPath, ec);
            if (ec)
            {
                LMS_LOG(DBUPDATER, ERROR, "Cannot get relative file path for '" << file.string() << "' from '" << rootPath.string() << "': " << ec.message());
                res.reset();
                return res;
            }
        }

        {
            std::error_code ec;
            res->fileSize = std::filesystem::file_size(file, ec);
            if (ec)
            {
                LMS_LOG(DBUPDATER, ERROR, "Cannot get file size for '" << file.string() << "': " << ec.message());
                res.reset();
                return res;
            }
        }

        return res;
    }

    db::Directory::pointer getOrCreateDirectory(db::Session& session, const std::filesystem::path& path, const db::MediaLibrary::pointer& mediaLibrary)
    {
        db::Directory::pointer directory{ db::Directory::find(session, path) };
        if (!directory)
        {
            db::Directory::pointer parentDirectory;
            if (path != mediaLibrary->getPath())
                parentDirectory = getOrCreateDirectory(session, path.parent_path(), mediaLibrary);

            directory = session.create<db::Directory>(path);
            directory.modify()->setParent(parentDirectory);
            directory.modify()->setMediaLibrary(mediaLibrary);
        }
        // Don't update library if it does not match, will be updated elsewhere

        return directory;
    }
} // namespace lms::scanner::utils