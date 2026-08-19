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

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/GenreId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"

namespace lms::db
{
    class Session;
    class Track;

    class Genre final : public Object<Genre, GenreId>
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

        Genre() = default;

        static std::size_t getCount(Session& session);
        static std::vector<GenreId> findIds(Session& session, const FindParameters& params);
        static std::vector<pointer> find(Session& session, const FindParameters& params);
        static void find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func);
        static pointer find(Session& session, GenreId id);
        static pointer find(Session& session, std::string_view name);
        static std::vector<GenreId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt);

        static std::size_t computeTrackCount(Session& session, GenreId id);
        static std::size_t computeReleaseCount(Session& session, GenreId id);

        std::string_view getName() const { return _name; }
        std::size_t getTrackCount() const { return _trackCount; }
        std::size_t getReleaseCount() const { return _releaseCount; }

        void setTrackCount(std::size_t count) { _trackCount = count; }
        void setReleaseCount(std::size_t count) { _releaseCount = count; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _trackCount, "track_count");
            Wt::Dbo::field(a, _releaseCount, "release_count");
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_genre", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Genre(std::string_view name);
        static pointer create(Session& session, std::string_view name);

        std::string _name;
        int _trackCount{};
        int _releaseCount{};

        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };

} // namespace lms::db
