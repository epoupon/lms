/*
 * Copyright (C) 2025 Emeric Poupon
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
#include <optional>
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ArtistInfoId.hpp"
#include "database/objects/DirectoryId.hpp"

namespace lms::db
{
    class Artist;
    class Directory;
    class Session;

    class ArtistInfo final : public Object<ArtistInfo, ArtistInfoId>
    {
    public:
        ArtistInfo() = default;

        // find
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, ArtistInfoId id);
        static void find(Session& session, ArtistId id, std::optional<Range> range, const std::function<void(const pointer&)>& func);
        static void find(Session& session, ArtistId id, const std::function<void(const pointer&)>& func);
        static pointer find(Session& session, const std::filesystem::path& path);
        static void find(Session& session, ArtistInfoId& lastRetrievedId, std::size_t count, const std::function<void(const pointer&)>& func);
        static void findArtistNameNoLongerMatch(Session& session, std::optional<Range> range, const std::function<void(const pointer&)>& func);
        static void findWithArtistNameAmbiguity(Session& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const pointer&)>& func);
        static void findAbsoluteFilePath(Session& session, ArtistInfoId& lastRetrievedId, std::size_t count, const std::function<void(ArtistInfoId artistInfoId, const std::filesystem::path& absoluteFilePath)>& func);

        // getters
        std::size_t getScanVersion() const { return _scanVersion; }
        const std::filesystem::path& getAbsoluteFilePath() const { return _absoluteFilePath; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        ObjectPtr<Directory> getDirectory() const;
        ObjectPtr<Artist> getArtist() const;
        DirectoryId getDirectoryId() const { return _directory.id(); }
        std::string_view getName() const { return _name; }
        std::string_view getSortName() const { return _name; }
        std::string_view getType() const { return _type; }
        std::string_view getGender() const { return _gender; }
        std::string_view getDisambiguation() const { return _disambiguation; }
        std::string_view getBiography() const { return _biography; }
        bool isMBIDMatched() const { return _MBIDMatched; }

        // setters
        void setScanVersion(std::size_t version) { _scanVersion = version; }
        void setAbsoluteFilePath(const std::filesystem::path& filePath);
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setDirectory(ObjectPtr<Directory> directory);
        void setArtist(ObjectPtr<Artist> artist);
        void setName(std::string_view name) { _name = name; }
        void setSortName(std::string_view sortName) { _sortName = sortName; }
        void setType(std::string_view type) { _type = type; }
        void setGender(std::string_view gender) { _gender = gender; }
        void setDisambiguation(std::string_view disambiguation) { _disambiguation = disambiguation; }
        void setBiography(std::string_view biography) { _biography = biography; };
        void setMBIDMatched(bool matched) { _MBIDMatched = matched; }

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

    private:
        friend class Session;
        static pointer create(Session& session);

        int _scanVersion{};

        // Set when coming from artist info file
        std::filesystem::path _absoluteFilePath;
        std::string _fileStem;
        Wt::WDateTime _fileLastWrite;

        // this info may be redondant with what found in the linked artist
        // but we actually need them in case of artist merge/split
        std::string _name;
        std::string _sortName;

        std::string _type;
        std::string _gender;
        std::string _disambiguation;
        std::string _biography;

        bool _MBIDMatched{};

        Wt::Dbo::ptr<Directory> _directory;
        Wt::Dbo::ptr<Artist> _artist;
    };
} // namespace lms::db
