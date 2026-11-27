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

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/GenreId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class Genre : public detail::Object<Genre, GenreId>
    {
    public:
        static constexpr std::size_t maxNameLength{ 512 };

        struct FindParameters
        {
            std::optional<Range> range;
            GenreSortMethod sortMethod{ GenreSortMethod::None };
            ArtistId artist;   // if set, genres of tracks by this artist
            TrackId track;     // if set, genres of this track
            ReleaseId release; // if set, genres involved in this release

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setSortMethod(GenreSortMethod _method)
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

        static Genre create(ISession& session, std::string_view name);
        static void destroy(ISession& session, std::span<const GenreId> ids);

        static std::size_t getCount(ISession& session);
        static std::vector<GenreId> findIds(ISession& session, const FindParameters& params);
        static std::vector<Genre> find(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const Genre&)>& func);
        static std::optional<Genre> find(ISession& session, GenreId id);
        static std::optional<Genre> find(ISession& session, std::string_view name);
        static std::vector<GenreId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        static std::size_t computeTrackCount(ISession& session, GenreId id);
        static std::size_t computeReleaseCount(ISession& session, GenreId id);

        static void update(ISession& session, const Genre& genre);

        std::string_view getName() const { return _name; }
        std::size_t getTrackCount() const { return _trackCount; }
        std::size_t getReleaseCount() const { return _releaseCount; }

        void setTrackCount(std::size_t count) { _trackCount = count; }
        void setReleaseCount(std::size_t count) { _releaseCount = count; }

    protected:
        Genre() = default;
        explicit Genre(std::string_view name);

        std::string _name;
        int _trackCount{};
        int _releaseCount{};
    };

} // namespace lms::db
