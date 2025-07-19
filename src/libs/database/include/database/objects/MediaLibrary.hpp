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
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>

#include "database/Object.hpp"
#include "database/objects/MediaLibraryId.hpp"

namespace lms::db
{
    class Session;

    class MediaLibrary final : public Object<MediaLibrary, MediaLibraryId>
    {
    public:
        static const std::size_t maxNameLength{ 128 };

        MediaLibrary() = default;

        // find
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, MediaLibraryId id);
        static pointer find(Session& session, std::string_view name);
        static pointer find(Session& session, const std::filesystem::path& path);
        static void find(Session& session, std::function<void(const pointer&)> func);
        static std::vector<pointer> find(Session& session);

        // getters
        std::string_view getName() const { return _name; }
        const std::filesystem::path& getPath() const { return _path; }
        bool isEmpty() const;

        // setters
        void setName(std::string_view name) { _name = name; }
        void setPath(const std::filesystem::path& p);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _path, "path");
            Wt::Dbo::field(a, _name, "name");
        }

    private:
        friend class Session;
        MediaLibrary(std::string_view name, const std::filesystem::path& p);
        static pointer create(Session& session, std::string_view name, const std::filesystem::path& p);

        std::filesystem::path _path;
        std::string _name;
    };
} // namespace lms::db
