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

#include <Wt/Dbo/Field.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/DirectoryId.hpp"
#include "database/objects/ImageId.hpp"

namespace lms::db
{
    class Directory;
    class Session;

    class Image final : public Object<Image, ImageId>
    {
    public:
        Image() = default;

        struct FindParameters
        {
            std::optional<Range> range;
            std::string fileStem;  // if set, images with this file stem
            DirectoryId directory; // if set, images in this directory

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setFileStem(std::string_view _fileStem)
            {
                fileStem = _fileStem;
                return *this;
            }
            FindParameters& setDirectory(DirectoryId _directory)
            {
                directory = _directory;
                return *this;
            }
        };

        // find
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, ImageId id);
        static pointer find(Session& session, const std::filesystem::path& file);
        static RangeResults<pointer> find(Session& session, const FindParameters& params);
        static void find(Session& session, const FindParameters& parameters, const std::function<void(const Image::pointer&)>& func);
        static void find(Session& session, ImageId& lastRetrievedId, std::size_t count, const std::function<void(const Image::pointer&)>& func);
        static void findAbsoluteFilePath(Session& session, ImageId& lastRetrievedId, std::size_t count, const std::function<void(ImageId imageId, const std::filesystem::path& absoluteFilePath)>& func);

        // getters
        const std::filesystem::path& getAbsoluteFilePath() const { return _fileAbsolutePath; }
        std::string_view getFileStem() const { return _fileStem; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        std::size_t getFileSize() const { return _fileSize; }
        std::size_t getWidth() const { return _width; }
        std::size_t getHeight() const { return _height; }

        // setters
        void setAbsoluteFilePath(const std::filesystem::path& p);
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setFileSize(std::size_t fileSize) { _fileSize = fileSize; }
        void setWidth(std::size_t width) { _width = width; }
        void setHeight(std::size_t height) { _height = height; }
        void setDirectory(const ObjectPtr<Directory>& directory) { _directory = getDboPtr(directory); }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _fileAbsolutePath, "absolute_file_path");
            Wt::Dbo::field(a, _fileStem, "stem");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");
            Wt::Dbo::field(a, _fileSize, "file_size");

            Wt::Dbo::field(a, _width, "width");
            Wt::Dbo::field(a, _height, "height");

            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Image(const std::filesystem::path& p);
        static pointer create(Session& session, const std::filesystem::path& p);

        std::filesystem::path _fileAbsolutePath;
        std::string _fileStem;
        Wt::WDateTime _fileLastWrite;
        int _fileSize{};
        int _width{};
        int _height{};

        Wt::Dbo::ptr<Directory> _directory;
    };
} // namespace lms::db
