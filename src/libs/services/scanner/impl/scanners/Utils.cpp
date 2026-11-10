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

#include <system_error>

#include "core/Path.hpp"
#include "database/ISession.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"

namespace lms::scanner::utils
{
    db::Directory getOrCreateDirectory(db::ISession& session, const std::filesystem::path& path, const std::optional<db::MediaLibrary>& mediaLibrary)
    {
        // Don't update library if it does not match, will be updated elsewhere
        if (std::optional<db::Directory> directory{ db::Directory::find(session, path) })
            return *directory;

        db::DirectoryId parentDirectoryId;
        if (path != mediaLibrary->getPath())
            parentDirectoryId = getOrCreateDirectory(session, path.parent_path(), mediaLibrary).getId();

        db::Directory directory{ db::Directory::create(session, path) };
        directory.setParent(parentDirectoryId);
        directory.setMediaLibrary(mediaLibrary->getId());
        db::Directory::update(session, directory);

        return directory;
    }
} // namespace lms::scanner::utils