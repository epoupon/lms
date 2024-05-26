/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <memory>

#include <Wt/WTemplate.h>

#include "common/ValueStringModel.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"

namespace lms::db
{
    class Artist;
}

namespace lms::ui
{
    using ArtistLinkTypesModel = ValueStringModel<std::optional<db::TrackArtistLinkType>>;

    namespace ArtistListHelpers
    {
        std::unique_ptr<Wt::WTemplate> createEntry(const db::ObjectPtr<db::Artist>& artist);
    }
} // namespace lms::ui
