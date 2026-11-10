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
#include <Wt/Dbo/collection.h>
#include <Wt/Dbo/weak_ptr.h>

#include "database/objects/PlayListFile.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Artwork;
    class Directory;
    class MediaLibrary;
    class TrackList;

    class PlayListFile final : public db::PlayListFile, public Wt::Dbo::Dbo<PlayListFile>
    {
    public:
        PlayListFile() = default;
        using db::PlayListFile::PlayListFile;

        void syncIds()
        {
            _id = id();
            _preferredArtworkId = _preferredArtwork.id();
            _directoryId = _directory.id();
            _mediaLibraryId = _mediaLibrary.id();
        }
        void applyIds();

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _absoluteFilePath, "absolute_file_path");
            Wt::Dbo::field(a, _fileStem, "file_stem");
            Wt::Dbo::field(a, _fileSize, "file_size");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _entries, "entries");
            Wt::Dbo::field(a, _coverImageFile, "cover_image_file");

            Wt::Dbo::belongsTo(a, _mediaLibrary, "media_library", Wt::Dbo::OnDeleteSetNull); // don't delete playlist on media library removal, we want to wait for the next scan to have a chance to migrate files
            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _preferredArtwork, "preferred_artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::hasOne(a, _trackList, "playlist_file");
        }

        Wt::Dbo::ptr<MediaLibrary> _mediaLibrary;
        Wt::Dbo::ptr<Directory> _directory;
        Wt::Dbo::ptr<Artwork> _preferredArtwork;
        Wt::Dbo::weak_ptr<TrackList> _trackList;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<PlayListFile>
    {
        using type = dbo::PlayListFile;
    };
} // namespace lms::db
