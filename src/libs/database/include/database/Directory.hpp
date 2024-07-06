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

#include <Wt/Dbo/Dbo.h>

#include "core/EnumSet.hpp"
#include "database/ArtistId.hpp"
#include "database/DirectoryId.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"

namespace lms::db
{
    class Session;

    class Directory final : public Object<Directory, DirectoryId>
    {
    public:
        Directory() = default;

        struct FindParameters
        {
            std::optional<Range> range;
            ArtistId artist;                                         // only tracks that involve this artist
            core::EnumSet<TrackArtistLinkType> trackArtistLinkTypes; // and for these link types

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setArtist(ArtistId _artist, core::EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {})
            {
                artist = _artist;
                trackArtistLinkTypes = _trackArtistLinkTypes;
                return *this;
            }
        };

        // find
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, DirectoryId id);
        static pointer find(Session& session, const std::filesystem::path& path);
        static void find(Session& session, DirectoryId& lastRetrievedDirectory, std::size_t count, const std::function<void(const Directory::pointer&)>& func);
        static void find(Session& session, const FindParameters& parameters, const std::function<void(const Directory::pointer&)>& func);
        static RangeResults<DirectoryId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt);

        // getters
        const std::filesystem::path& getAbsolutePath() const { return _absolutePath; }
        std::string_view getName() const { return _name; }
        ObjectPtr<Directory> getParent() const { return _parent; }

        // setters
        void setAbsolutePath(const std::filesystem::path& p);
        void setParent(ObjectPtr<Directory> parent);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _absolutePath, "absolute_path");
            Wt::Dbo::field(a, _name, "name");

            Wt::Dbo::belongsTo(a, _parent, "parent_directory", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Directory(const std::filesystem::path& p);
        static pointer create(Session& session, const std::filesystem::path& p);

        std::filesystem::path _absolutePath;
        std::string _name;

        Wt::Dbo::ptr<Directory> _parent;
    };
} // namespace lms::db
