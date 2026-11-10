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

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>

#include "database/objects/Directory.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class MediaLibrary;

    class Directory final : public db::Directory, public Wt::Dbo::Dbo<Directory>
    {
    public:
        Directory() = default;
        using db::Directory::Directory;

        void syncIds()
        {
            _id = id();
            _parentDirectoryId = _parent.id();
            _mediaLibraryId = _mediaLibrary.id();
        }
        void applyIds();

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _absolutePath, "absolute_path");
            Wt::Dbo::field(a, _name, "name");

            Wt::Dbo::belongsTo(a, _parent, "parent_directory", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _mediaLibrary, "media_library", Wt::Dbo::OnDeleteSetNull); // don't delete directories on media library removal, we want to wait for the next scan to have a chance to migrate files
        }

        Wt::Dbo::ptr<Directory> _parent;
        Wt::Dbo::ptr<MediaLibrary> _mediaLibrary;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Directory>
    {
        using type = dbo::Directory;
    };
} // namespace lms::db
