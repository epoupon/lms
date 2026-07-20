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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "core/UUID.hpp"

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/WorkId.hpp"

namespace lms::db
{
    class Session;
    class Track;

    class Work final : public Object<Work, WorkId>
    {
    public:
        static constexpr std::size_t maxNameLength{ 512 };

        Work() = default;

        static pointer find(Session& session, WorkId id);
        // Global lookup: MusicBrainz Work Id is a strong, unambiguous identity shared across the whole library
        static pointer find(Session& session, const core::UUID& mbid);
        // Name-only lookup, scoped to works already linked to a track of the given release: work titles are
        // often generic (e.g. "Symphony No. 5") and collide across unrelated works, so without an mbid we only
        // ever match within the same release instead of matching globally by name
        static pointer find(Session& session, ReleaseId releaseId, std::string_view name);
        static std::vector<WorkId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt);

        void setName(std::string_view name);

        std::string_view getName() const { return _name; }
        std::optional<core::UUID> getMBID() const { return _mbid; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _mbid, "mbid");
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_work", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Work(std::string_view name, const std::optional<core::UUID>& mbid);
        static pointer create(Session& session, std::string_view name, const std::optional<core::UUID>& mbid);

        std::string _name;
        std::optional<core::UUID> _mbid;
        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };

} // namespace lms::db
