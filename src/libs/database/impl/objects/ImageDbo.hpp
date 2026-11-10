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

#include "database/objects/Image.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Directory;

    class Image final : public db::Image, public Wt::Dbo::Dbo<Image>
    {
    public:
        Image() = default;
        using db::Image::Image;

        void syncIds()
        {
            _id = id();
            _directoryId = _directory.id();
        }
        void applyIds();

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _fileAbsolutePath, "absolute_file_path");
            Wt::Dbo::field(a, _fileStem, "stem");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");
            Wt::Dbo::field(a, _fileSize, "file_size");

            Wt::Dbo::field(a, _width, "width");
            Wt::Dbo::field(a, _height, "height");
            Wt::Dbo::field(a, _format, "format");

            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Directory> _directory;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Image>
    {
        using type = dbo::Image;
    };
} // namespace lms::db
