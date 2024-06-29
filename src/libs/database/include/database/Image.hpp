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

#include <Wt/WDateTime.h>
#include <Wt/Dbo/Dbo.h>

#include "database/ArtistId.hpp"
#include "database/ImageId.hpp"
#include "database/Object.hpp"

namespace lms::db
{
    class Artist;
    class Session;

    class Image final : public Object<Image, ImageId>
    {
    public:
        Image() = default;

        // find
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, ImageId id);

        // getters
        const std::filesystem::path& getPath() const { return _path; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        std::size_t getFileSize() const { return _fileSize; }
        std::size_t getWidth() const { return _width; }
        std::size_t getHeight() const { return _height; }

        // setters
        void setPath(const std::filesystem::path& p) { _path = p; }
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setFileSize(std::size_t fileSize) { _fileSize = fileSize; }
        void setWidth(std::size_t width) { _width = width; }
        void setHeight(std::size_t height) { _height = height; }
        void setArtist(const ObjectPtr<Artist>& artist) { _artist = getDboPtr(artist); }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _path, "path");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");
            Wt::Dbo::field(a, _fileSize, "file_size");

            Wt::Dbo::field(a, _width, "width");
            Wt::Dbo::field(a, _height, "height");

            Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Image(const std::filesystem::path& p);
        static pointer create(Session& session, const std::filesystem::path& p);

        std::filesystem::path _path;
        Wt::WDateTime _fileLastWrite;
        int _fileSize{};
        int _width{};
        int _height{};

        Wt::Dbo::ptr<Artist> _artist;
    };
} // namespace lms::db
