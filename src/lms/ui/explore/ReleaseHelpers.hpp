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
#include <optional>

#include <Wt/WString.h>
#include <Wt/WTemplate.h>
#include <Wt/WDate.h>
#include "services/database/Object.hpp"
#include "services/database/Types.hpp"
#include "utils/EnumSet.hpp"

namespace Database
{
    class Artist;
    class Release;
}

namespace UserInterface::ReleaseListHelpers
{
    std::unique_ptr<Wt::WTemplate> createEntry(const Database::ObjectPtr<Database::Release>& release);
    std::unique_ptr<Wt::WTemplate> createEntryForArtist(const Database::ObjectPtr<Database::Release>& release, const Database::ObjectPtr<Database::Artist>& artist);
} // namespace UserInterface

namespace UserInterface::ReleaseHelpers
{
    Wt::WString buildReleaseTypeString(Database::ReleaseTypePrimary primaryType, EnumSet<Database::ReleaseTypeSecondary> secondaryTypes);
    Wt::WString buildReleaseYearString(const Wt::WDate& releaseDate, const Wt::WDate& originalReleaseDate);
}
