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
#include "database/objects/LanguageId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class Language : public detail::Object<Language, LanguageId>
    {
    public:
        static constexpr std::size_t maxNameLength{ 512 };

        struct FindParameters
        {
            std::optional<Range> range;
            LanguageSortMethod sortMethod{ LanguageSortMethod::None };
            ArtistId artist;   // if set, languages of tracks by this artist
            TrackId track;     // if set, languages of this track
            ReleaseId release; // if set, languages involved in this release

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setSortMethod(LanguageSortMethod _method)
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

        static Language create(ISession& session, std::string_view name);
        static void destroy(ISession& session, std::span<const LanguageId> ids);

        static std::size_t getCount(ISession& session);
        static std::vector<LanguageId> findIds(ISession& session, const FindParameters& params);
        static std::vector<Language> find(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const Language&)>& func);
        static std::optional<Language> find(ISession& session, LanguageId id);
        static std::optional<Language> find(ISession& session, std::string_view name);
        static std::vector<LanguageId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        std::string_view getName() const { return _name; }

    protected:
        Language() = default;
        explicit Language(std::string_view name);

        std::string _name;
    };

} // namespace lms::db
