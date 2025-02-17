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

#include <Wt/WDate.h>
#include <Wt/WString.h>
#include <Wt/WTemplate.h>

#include "database/Object.hpp"

#include "ReleaseTypes.hpp"

namespace lms::db
{
    class Artist;
    class Release;
} // namespace lms::db

namespace lms::ui::releaseListHelpers
{
    std::unique_ptr<Wt::WTemplate> createEntry(const db::ObjectPtr<db::Release>& release);
    std::unique_ptr<Wt::WTemplate> createEntryForArtist(const db::ObjectPtr<db::Release>& release, const db::ObjectPtr<db::Artist>& artist);
    std::unique_ptr<Wt::WTemplate> createEntryForOtherVersions(const db::ObjectPtr<db::Release>& release);
} // namespace lms::ui::releaseListHelpers

namespace lms::ui::releaseHelpers
{
    Wt::WString buildReleaseTypeString(const ReleaseType& releaseType);
    Wt::WString buildReleaseYearString(std::optional<int> year, std::optional<int> originalYear);
} // namespace lms::ui::releaseHelpers
