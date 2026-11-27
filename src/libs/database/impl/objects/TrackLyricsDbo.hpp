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

#include "database/objects/TrackLyrics.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Directory;
    class Track;

    class TrackLyrics final : public db::TrackLyrics, public Wt::Dbo::Dbo<TrackLyrics>
    {
    public:
        TrackLyrics() = default;
        using db::TrackLyrics::TrackLyrics;

        void syncIds()
        {
            _id = id();
            _trackId = _track.id();
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
            Wt::Dbo::field(a, _lines, "lines");
            Wt::Dbo::field(a, _language, "language");
            Wt::Dbo::field(a, _offset, "offset");
            Wt::Dbo::field(a, _displayArtist, "display_artist");
            Wt::Dbo::field(a, _displayTitle, "display_title");
            Wt::Dbo::field(a, _synchronized, "synchronized");
            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Track> _track;
        Wt::Dbo::ptr<Directory> _directory;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<TrackLyrics>
    {
        using type = dbo::TrackLyrics;
    };
} // namespace lms::db
