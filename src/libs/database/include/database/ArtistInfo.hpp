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

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "database/ArtistId.hpp"
#include "database/ArtistInfoId.hpp"
#include "database/DirectoryId.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"

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

        // getters
        const std::filesystem::path& getAbsoluteFilePath() const { return _absoluteFilePath; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        ObjectPtr<Directory> getDirectory() const;
        ObjectPtr<Artist> getArtist() const;
        DirectoryId getDirectoryId() const { return _directory.id(); }
        std::string_view getType() const { return _type; }
        std::string_view getGender() const { return _gender; }
        std::string_view getDisambiguation() const { return _disambiguation; }
        std::string_view getBiography() const { return _biography; }

        // setters
        void setAbsoluteFilePath(const std::filesystem::path& filePath);
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setDirectory(ObjectPtr<Directory> directory);
        void setArtist(ObjectPtr<Artist> artist);
        void setType(std::string_view type) { _type = type; }
        void setGender(std::string_view gender) { _gender = gender; }
        void setDisambiguation(std::string_view disambiguation) { _disambiguation = disambiguation; }
        void setBiography(std::string_view biography) { _biography = biography; };

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _absoluteFilePath, "absolute_file_path");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");

            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _gender, "gender");
            Wt::Dbo::field(a, _disambiguation, "disambiguation");
            Wt::Dbo::field(a, _biography, "biography");

            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        static pointer create(Session& session);

        // Set when coming from artist info file
        std::filesystem::path _absoluteFilePath;
        std::string _fileStem;
        Wt::WDateTime _fileLastWrite;

        std::string _type;
        std::string _gender;
        std::string _disambiguation;
        std::string _biography;

        Wt::Dbo::ptr<Directory> _directory;
        Wt::Dbo::ptr<Artist> _artist;
    };
} // namespace lms::db
