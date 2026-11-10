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
#include <span>
#include <string>
#include <string_view>

#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/GroupingId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class Grouping : public detail::Object<Grouping, GroupingId>
    {
    public:
        static constexpr std::size_t maxNameLength{ 512 };

        struct FindParameters
        {
            std::optional<Range> range;
            GroupingSortMethod sortMethod{ GroupingSortMethod::None };
            ArtistId artist;   // if set, groupings of tracks by this artist
            TrackId track;     // if set, groupings of this track
            ReleaseId release; // if set, groupings involved in this release

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setSortMethod(GroupingSortMethod _method)
            {
                sortMethod = _method;
                return *this;
            }
            FindParameters& setTrack(TrackId _track)
            {
                track = _track;
                return *this;
            }
            FindParameters& setArtist(ArtistId _artist)
            {
                artist = _artist;
                return *this;
            }
            FindParameters& setRelease(ReleaseId _release)
            {
                release = _release;
                return *this;
            }
        };

        static Grouping create(ISession& session, std::string_view name);
        static void destroy(ISession& session, std::span<const GroupingId> ids);

        static std::size_t getCount(ISession& session);
        static std::vector<GroupingId> findIds(ISession& session, const FindParameters& params);
        static std::vector<Grouping> find(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const Grouping&)>& func);
        static std::optional<Grouping> find(ISession& session, GroupingId id);
        static std::optional<Grouping> find(ISession& session, std::string_view name);
        static std::vector<GroupingId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        std::string_view getName() const { return _name; }

    protected:
        Grouping() = default;
        explicit Grouping(std::string_view name);

        std::string _name;
    };

} // namespace lms::db
