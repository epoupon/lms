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

#include "database/objects/ArtistInfo.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Artist;
    class Directory;

    class ArtistInfo final : public db::ArtistInfo, public Wt::Dbo::Dbo<ArtistInfo>
    {
    public:
        ArtistInfo() = default;
        using db::ArtistInfo::ArtistInfo;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _scanVersion, "scan_version");
            Wt::Dbo::field(a, _absoluteFilePath, "absolute_file_path");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");

            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _sortName, "sort_name");
            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _gender, "gender");
            Wt::Dbo::field(a, _disambiguation, "disambiguation");
            Wt::Dbo::field(a, _biography, "biography");

            Wt::Dbo::field(a, _MBIDMatched, "mbid_matched");

            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Directory> _directory;
        Wt::Dbo::ptr<Artist> _artist;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<ArtistInfo>
    {
        using type = dbo::ArtistInfo;
    };
} // namespace lms::db
