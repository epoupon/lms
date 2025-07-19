/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "DatabaseCollectorBase.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"

namespace lms::db
{
    class Artist;
}

namespace lms::ui
{
    class ArtistCollector : public DatabaseCollectorBase
    {
    public:
        using DatabaseCollectorBase::DatabaseCollectorBase;

        db::RangeResults<db::ArtistId> get(std::optional<db::Range> range = std::nullopt);
        void reset() { _randomArtists.reset(); }
        void setArtistLinkType(std::optional<db::TrackArtistLinkType> linkType) { _linkType = linkType; }

    private:
        db::RangeResults<db::ArtistId> getRandomArtists(Range range);
        std::optional<db::RangeResults<db::ArtistId>> _randomArtists;
        std::optional<db::TrackArtistLinkType> _linkType;
    };
} // namespace lms::ui
