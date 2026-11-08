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

#include <Wt/WDateTime.h>

#include "database/IdRange.hpp"
#include "database/Object.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/DirectoryId.hpp"

LMS_DECLARE_IDTYPE(PlayListFileId)

namespace lms::db
{
    class Directory;
    class ISession;
    class MediaLibrary;
    class TrackList;

    class PlayListFile : public Object<PlayListFile, PlayListFileId>
    {
    public:
        static pointer create(ISession& session, const std::filesystem::path& file);
        static void destroy(ISession& session, std::span<const PlayListFileId> ids);

        // find
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, PlayListFileId id);
        static pointer find(ISession& session, const std::filesystem::path& path);
        static void find(ISession& session, PlayListFileId& lastRetrievedId, std::size_t count, const std::function<void(const pointer&)>& func);
        static void findAbsoluteFilePath(ISession& session, PlayListFileId& lastRetrievedId, std::size_t count, const std::function<void(PlayListFileId playListFileId, const std::filesystem::path& absoluteFilePath)>& func);
        static void find(ISession& session, const IdRange<PlayListFileId>& idRange, const std::function<void(const PlayListFile::pointer&)>& func);
        static IdRange<PlayListFileId> findNextIdRange(ISession& session, PlayListFileId lastRetrievedId, std::size_t count);

        // getters
        const std::filesystem::path& getAbsoluteFilePath() const { return _absoluteFilePath; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        std::size_t getFileSize() const { return _fileSize; }
        std::string_view getName() const { return _name; }
        const std::filesystem::path& getCoverImageFile() const { return _coverImageFile; }
        ArtworkId getPreferredArtworkId() const;
        std::vector<std::filesystem::path> getFiles() const;
        ObjectPtr<TrackList> getTrackList() const;
        ObjectPtr<Directory> getDirectory() const;
        DirectoryId getDirectoryId() const;

        // setters
        void setAbsoluteFilePath(const std::filesystem::path& filePath);
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setFileSize(std::size_t fileSize) { _fileSize = fileSize; }
        void setCoverImageFile(const std::filesystem::path& file);
        void setMediaLibrary(const ObjectPtr<MediaLibrary>& mediaLibrary);
        void setDirectory(const ObjectPtr<Directory>& directory);
        void setTrackList(const ObjectPtr<TrackList>& trackList);
        void setName(std::string_view name);
        void setFiles(std::span<const std::filesystem::path> files);

        static void updatePreferredArtwork(ISession& session, PlayListFileId id, ArtworkId artworkId);

    protected:
        PlayListFile() = default;
        explicit PlayListFile(const std::filesystem::path& file);

        static constexpr std::size_t _maxNameLength{ 512 };

        std::filesystem::path _absoluteFilePath;
        std::string _fileStem;
        Wt::WDateTime _fileLastWrite;
        Wt::WDateTime _fileAdded;
        long long _fileSize{};

        std::string _name;
        std::string _entries; // A json encoded list of files
        std::filesystem::path _coverImageFile;
    };
} // namespace lms::db
