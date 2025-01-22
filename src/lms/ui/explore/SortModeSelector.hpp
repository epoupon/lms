/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "DropDownMenuSelector.hpp"

#include "DatabaseCollectorBase.hpp"

namespace lms::ui
{
    class SortModeSelector : public DropDownMenuSelector<DatabaseCollectorBase::Mode>
    {
    public:
        SortModeSelector(DatabaseCollectorBase::Mode defaultMode)
            : DropDownMenuSelector<DatabaseCollectorBase::Mode>{ Wt::WString::tr("Lms.Explore.template.sort-mode-selector"), defaultMode }
        {
            bindItem("random", Wt::WString::tr("Lms.Explore.random"), DatabaseCollectorBase::Mode::Random);
            bindItem("starred", Wt::WString::tr("Lms.Explore.starred"), DatabaseCollectorBase::Mode::Starred);
            bindItem("recently-played", Wt::WString::tr("Lms.Explore.recently-played"), DatabaseCollectorBase::Mode::RecentlyPlayed);
            bindItem("most-played", Wt::WString::tr("Lms.Explore.most-played"), DatabaseCollectorBase::Mode::MostPlayed);
            bindItem("recently-added", Wt::WString::tr("Lms.Explore.recently-added"), DatabaseCollectorBase::Mode::RecentlyAdded);
            bindItem("recently-modified", Wt::WString::tr("Lms.Explore.recently-modified"), DatabaseCollectorBase::Mode::RecentlyModified);
            bindItem("all", Wt::WString::tr("Lms.Explore.all"), DatabaseCollectorBase::Mode::All);
        }
    };
} // namespace lms::ui
