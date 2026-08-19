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

#include <functional>
#include <optional>
#include <vector>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"

#include "ArtistType.hpp"
#include "DatabaseCollectorBase.hpp"

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

        void get(db::Range range, bool& moreResults, const std::function<void(const db::ObjectPtr<db::Artist>&)>& func);
        void reset() { _randomArtists.reset(); }
        void setArtistType(ArtistType artistType) { _artistType = artistType; }

    private:
        std::optional<std::vector<db::ArtistId>> _randomArtists;
        ArtistType _artistType;
    };
} // namespace lms::ui
