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
#include "database/Session.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"

namespace lms::scanner::utils
{
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