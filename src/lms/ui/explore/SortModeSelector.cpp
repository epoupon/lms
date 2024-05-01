/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "SortModeSelector.hpp"

#include <Wt/WPushButton.h>

namespace lms::ui
{
    SortModeSelector::SortModeSelector(DatabaseCollectorBase::Mode defaultMode)
        : Wt::WTemplate{ Wt::WString::tr("Lms.Explore.template.sort-mode-selector") }
    {
        auto* sortMode{ bindNew<Wt::WText>("sort-mode") };

        auto bindMenuItem{ [this, sortMode, defaultMode](const std::string& var, const Wt::WString& title, DatabaseCollectorBase::Mode mode)
        {
            auto* menuItem {bindNew<Wt::WPushButton>(var, title)};
            menuItem->clicked().connect([this, mode, menuItem, sortMode, title]
            {
                _currentActiveItem->removeStyleClass("active");
                menuItem->addStyleClass("active");
                _currentActiveItem = menuItem;
                sortMode->setText(title);

                sortModeChanged.emit(mode);
            });

            if (mode == defaultMode)
            {
                _currentActiveItem = menuItem;
                _currentActiveItem->addStyleClass("active");
                sortMode->setText(title);
            }
        } };

        bindMenuItem("random", Wt::WString::tr("Lms.Explore.random"), DatabaseCollectorBase::Mode::Random);
        bindMenuItem("starred", Wt::WString::tr("Lms.Explore.starred"), DatabaseCollectorBase::Mode::Starred);
        bindMenuItem("recently-played", Wt::WString::tr("Lms.Explore.recently-played"), DatabaseCollectorBase::Mode::RecentlyPlayed);
        bindMenuItem("most-played", Wt::WString::tr("Lms.Explore.most-played"), DatabaseCollectorBase::Mode::MostPlayed);
        bindMenuItem("recently-added", Wt::WString::tr("Lms.Explore.recently-added"), DatabaseCollectorBase::Mode::RecentlyAdded);
        bindMenuItem("all", Wt::WString::tr("Lms.Explore.all"), DatabaseCollectorBase::Mode::All);
    }
} // namespace lms::ui
