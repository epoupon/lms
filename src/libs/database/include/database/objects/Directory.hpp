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
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Field.h>

#include "core/EnumSet.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/DirectoryId.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/ReleaseId.hpp"

namespace lms::db
{
    class Session;
    class MediaLibrary;

    class Directory final : public Object<Directory, DirectoryId>
    {
    public:
        Directory() = default;

        struct FindParameters
        {
            std::optional<Range> range;
            std::vector<std::string_view> keywords;                  // if non empty, name must match all of these keywords
            ArtistId artist;                                         // only directory that involve this artist
            ReleaseId release;                                       // only releases that involve this artist
            core::EnumSet<TrackArtistLinkType> trackArtistLinkTypes; // and for these link types
            DirectoryId parentDirectory;                             // If set, directories that have this parent
            bool withNoTrack{};                                      // If set, directories that do not contain any track
            MediaLibraryId mediaLibrary;                             // If set, directories in this library
            DirectorySortMethod sortMethod{ DirectorySortMethod::None };

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setKeywords(const std::vector<std::string_view>& _keywords)
            {
                keywords = _keywords;
                return *this;
            }
            FindParameters& setArtist(ArtistId _artist, core::EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {})
            {
                artist = _artist;
                trackArtistLinkTypes = _trackArtistLinkTypes;
                return *this;
            }
            FindParameters& setRelease(ReleaseId _release)
            {
                release = _release;
                return *this;
            }
            FindParameters& setParentDirectory(DirectoryId _parentDirectory)
            {
                parentDirectory = _parentDirectory;
                return *this;
            }
            FindParameters& setWithNoTrack(bool _withNoTrack)
            {
                withNoTrack = _withNoTrack;
                return *this;
            }
            FindParameters& setMediaLibrary(MediaLibraryId _mediaLibrary)
            {
                mediaLibrary = _mediaLibrary;
                return *this;
            }
            FindParameters& setSortMethod(DirectorySortMethod _method)
            {
                sortMethod = _method;
                return *this;
            }
        };

        // find
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, DirectoryId id);
        static pointer find(Session& session, const std::filesystem::path& path);
        static void find(Session& session, DirectoryId& lastRetrievedDirectory, std::size_t count, const std::function<void(const Directory::pointer&)>& func);
        static RangeResults<Directory::pointer> find(Session& session, const FindParameters& params);
        static void find(Session& session, const FindParameters& parameters, const std::function<void(const Directory::pointer&)>& func);
        static RangeResults<DirectoryId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt);
        static RangeResults<DirectoryId> findMismatchedLibrary(Session& session, std::optional<Range> range, const std::filesystem::path& rootPath, MediaLibraryId expectedLibraryId);
        static RangeResults<pointer> findRootDirectories(Session& session, std::optional<Range> range = std::nullopt);

        // getters
        const std::filesystem::path& getAbsolutePath() const { return _absolutePath; }
        std::string_view getName() const { return _name; }
        ObjectPtr<Directory> getParentDirectory() const { return _parent; }
        DirectoryId getParentDirectoryId() const { return _parent.id(); }
        ObjectPtr<MediaLibrary> getMediaLibrary() const { return _mediaLibrary; }

        // setters
        void setAbsolutePath(const std::filesystem::path& p);
        void setParent(ObjectPtr<Directory> parent);
        void setMediaLibrary(ObjectPtr<MediaLibrary> mediaLibrary) { _mediaLibrary = getDboPtr(mediaLibrary); }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _absolutePath, "absolute_path");
            Wt::Dbo::field(a, _name, "name");

            Wt::Dbo::belongsTo(a, _parent, "parent_directory", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _mediaLibrary, "media_library", Wt::Dbo::OnDeleteSetNull); // don't delete directories on media library removal, we want to wait for the next scan to have a chance to migrate files
        }

    private:
        friend class Session;
        Directory(const std::filesystem::path& p);
        static pointer create(Session& session, const std::filesystem::path& p);

        std::filesystem::path _absolutePath;
        std::string _name;

        Wt::Dbo::ptr<Directory> _parent;
        Wt::Dbo::ptr<MediaLibrary> _mediaLibrary;
    };
} // namespace lms::db
