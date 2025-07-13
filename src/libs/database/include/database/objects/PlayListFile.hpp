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
#include <span>
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>
#include <Wt/Dbo/weak_ptr.h>
#include <Wt/WDateTime.h>

#include "database/IdRange.hpp"
#include "database/Object.hpp"
#include "database/objects/DirectoryId.hpp"

LMS_DECLARE_IDTYPE(PlayListFileId)

namespace lms::db
{
    class Session;
    class Directory;
    class MediaLibrary;
    class TrackList;

    class PlayListFile final : public Object<PlayListFile, PlayListFileId>
    {
    public:
        PlayListFile() = default;

        // find
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, PlayListFileId id);
        static pointer find(Session& session, const std::filesystem::path& path);
        static void find(Session& session, PlayListFileId& lastRetrievedId, std::size_t count, const std::function<void(const pointer&)>& func);
        static void findAbsoluteFilePath(Session& session, PlayListFileId& lastRetrievedId, std::size_t count, const std::function<void(PlayListFileId playListFileId, const std::filesystem::path& absoluteFilePath)>& func);
        static void find(Session& session, const IdRange<PlayListFileId>& idRange, const std::function<void(const PlayListFile::pointer&)>& func);
        static IdRange<PlayListFileId> findNextIdRange(Session& session, PlayListFileId lastRetrievedId, std::size_t count);

        // getters
        const std::filesystem::path& getAbsoluteFilePath() const { return _absoluteFilePath; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        std::size_t getFileSize() const { return _fileSize; }
        std::string_view getName() const { return _name; }
        std::vector<std::filesystem::path> getFiles() const;
        ObjectPtr<TrackList> getTrackList() const;
        ObjectPtr<Directory> getDirectory() const;
        DirectoryId getDirectoryId() const { return _directory.id(); }

        // setters
        void setAbsoluteFilePath(const std::filesystem::path& filePath);
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setFileSize(std::size_t fileSize) { _fileSize = fileSize; }
        void setMediaLibrary(ObjectPtr<MediaLibrary> mediaLibrary) { _mediaLibrary = getDboPtr(mediaLibrary); }
        void setDirectory(ObjectPtr<Directory> directory);
        void setTrackList(ObjectPtr<TrackList> trackList);
        void setName(std::string_view name);
        void setFiles(std::span<const std::filesystem::path> files);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _absoluteFilePath, "absolute_file_path");
            Wt::Dbo::field(a, _fileStem, "file_stem");
            Wt::Dbo::field(a, _fileSize, "file_size");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _entries, "entries");

            Wt::Dbo::belongsTo(a, _mediaLibrary, "media_library", Wt::Dbo::OnDeleteSetNull); // don't delete playlist on media library removal, we want to wait for the next scan to have a chance to migrate files
            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasOne(a, _trackList, "playlist_file");
        }

    private:
        friend class Session;
        PlayListFile(const std::filesystem::path& file);
        static pointer create(Session& session, const std::filesystem::path& file);

        static constexpr std::size_t _maxNameLength{ 512 };

        std::filesystem::path _absoluteFilePath;
        std::string _fileStem;
        Wt::WDateTime _fileLastWrite;
        Wt::WDateTime _fileAdded;
        long long _fileSize{};

        std::string _name;
        std::string _entries; // A json encoded list of files

        Wt::Dbo::ptr<MediaLibrary> _mediaLibrary;
        Wt::Dbo::ptr<Directory> _directory;
        Wt::Dbo::weak_ptr<TrackList> _trackList;
    };
} // namespace lms::db
