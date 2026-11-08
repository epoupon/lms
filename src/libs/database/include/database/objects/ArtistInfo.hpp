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
#include <span>
#include <string>
#include <string_view>

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
    class ISession;

    class ArtistInfo : public Object<ArtistInfo, ArtistInfoId>
    {
    public:
        static pointer create(ISession& session);
        static void destroy(ISession& session, std::span<const ArtistInfoId> ids);

        // find
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, ArtistInfoId id);
        static void find(ISession& session, ArtistId id, std::optional<Range> range, const std::function<void(const pointer&)>& func);
        static void find(ISession& session, ArtistId id, const std::function<void(const pointer&)>& func);
        static pointer find(ISession& session, const std::filesystem::path& path);
        static void find(ISession& session, ArtistInfoId& lastRetrievedId, std::size_t count, const std::function<void(const pointer&)>& func);
        static void findArtistNameNoLongerMatch(ISession& session, std::optional<Range> range, const std::function<void(const pointer&)>& func);
        static void findWithArtistNameAmbiguity(ISession& session, std::optional<Range> range, bool allowArtistMBIDFallback, const std::function<void(const pointer&)>& func);
        static void findAbsoluteFilePath(ISession& session, ArtistInfoId& lastRetrievedId, std::size_t count, const std::function<void(ArtistInfoId artistInfoId, const std::filesystem::path& absoluteFilePath)>& func);

        // getters
        std::size_t getScanVersion() const { return _scanVersion; }
        const std::filesystem::path& getAbsoluteFilePath() const { return _absoluteFilePath; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        ObjectPtr<Directory> getDirectory() const;
        ObjectPtr<Artist> getArtist() const;
        DirectoryId getDirectoryId() const;
        std::string_view getName() const { return _name; }
        std::string_view getSortName() const { return _sortName; }
        std::string_view getType() const { return _type; }
        std::string_view getGender() const { return _gender; }
        std::string_view getDisambiguation() const { return _disambiguation; }
        std::string_view getBiography() const { return _biography; }
        bool isMBIDMatched() const { return _MBIDMatched; }

        // setters
        void setScanVersion(std::size_t version) { _scanVersion = version; }
        void setAbsoluteFilePath(const std::filesystem::path& filePath);
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setDirectory(const ObjectPtr<Directory>& directory);
        void setArtist(const ObjectPtr<Artist>& artist);
        void setName(std::string_view name);
        void setSortName(std::string_view sortName);
        void setType(std::string_view type) { _type = type; }
        void setGender(std::string_view gender) { _gender = gender; }
        void setDisambiguation(std::string_view disambiguation) { _disambiguation = disambiguation; }
        void setBiography(std::string_view biography) { _biography = biography; };
        void setMBIDMatched(bool matched) { _MBIDMatched = matched; }

    protected:
        ArtistInfo() = default;
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
    };
} // namespace lms::db
