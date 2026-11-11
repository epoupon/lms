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
#include <span>

#include <Wt/WDateTime.h>

#include "core/TaggedType.hpp"
#include "core/media/ImageFormat.hpp"

#include "database/Types.hpp"
#include "database/objects/DirectoryId.hpp"
#include "database/objects/ImageId.hpp"
#include "database/objects/detail/Object.hpp"
#include "database/objects/detail/Types.hpp"

namespace lms::db
{
    class ISession;

    class Image : public detail::Object<Image, ImageId>
    {
    public:
        static Image create(ISession& session, const std::filesystem::path& p);
        static void destroy(ISession& session, std::span<const ImageId> ids);

        struct FindParameters
        {
            using ProcessWildcards = core::TaggedBool<class FindParametersProcessWildcardsTag>;

            std::optional<Range> range;
            std::string fileStem;                                 // if set, images with this file stem
            ProcessWildcards processWildcardsInFileStem{ false }; // if true, replace '*' by '%' for SQL LIKE
            DirectoryId directory;                                // if set, images in this directory

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setFileStem(std::string_view _fileStem, ProcessWildcards processWildcards = ProcessWildcards{ false })
            {
                fileStem = _fileStem;
                processWildcardsInFileStem = processWildcards;
                return *this;
            }
            FindParameters& setDirectory(DirectoryId _directory)
            {
                directory = _directory;
                return *this;
            }
        };

        // find
        static std::size_t getCount(ISession& session);
        static std::optional<Image> find(ISession& session, ImageId id);
        static std::optional<Image> find(ISession& session, const std::filesystem::path& file);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const Image&)>& func);
        static void find(ISession& session, ImageId& lastRetrievedId, std::size_t count, const std::function<void(const Image&)>& func);
        static void findAbsoluteFilePath(ISession& session, ImageId& lastRetrievedId, std::size_t count, const std::function<void(ImageId imageId, const std::filesystem::path& absoluteFilePath)>& func);

        static void update(ISession& session, const Image& image);

        // getters
        const std::filesystem::path& getAbsoluteFilePath() const { return _fileAbsolutePath; }
        std::string_view getFileStem() const { return _fileStem; }
        const Wt::WDateTime& getLastWriteTime() const { return _fileLastWrite; }
        std::size_t getFileSize() const { return _fileSize; }
        std::size_t getWidth() const { return _width; }
        std::size_t getHeight() const { return _height; }
        std::optional<core::media::ImageFormat> getFormat() const;

        // setters
        void setAbsoluteFilePath(const std::filesystem::path& p);
        void setLastWriteTime(Wt::WDateTime time) { _fileLastWrite = time; }
        void setFileSize(std::size_t fileSize) { _fileSize = fileSize; }
        void setWidth(std::size_t width) { _width = width; }
        void setHeight(std::size_t height) { _height = height; }
        void setFormat(core::media::ImageFormat format);
        void setDirectory(DirectoryId directory) { _directoryId = directory; }

    protected:
        Image() = default;
        explicit Image(const std::filesystem::path& p);

        std::filesystem::path _fileAbsolutePath;
        std::string _fileStem;
        Wt::WDateTime _fileLastWrite;
        int _fileSize{};
        int _width{};
        int _height{};
        detail::ImageFormat _format{ detail::ImageFormat::Unknown };
        DirectoryId _directoryId;
    };
} // namespace lms::db
