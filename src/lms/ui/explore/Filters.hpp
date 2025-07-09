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

#pragma once

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>
#include <Wt/WTemplate.h>

#include "database/objects/Filters.hpp"

namespace lms::ui
{
    class Filters : public Wt::WTemplate
    {
    public:
        Filters();

        const db::Filters& getDbFilters() const { return _dbFilters; }

        void add(db::ClusterId clusterId);

        Wt::Signal<>& updated() { return _sigUpdated; }

    private:
        void showDialog();
        void set(db::LabelId labelId);
        void set(db::MediaLibraryId mediaLibraryId);
        void set(db::ReleaseTypeId releaseTypeId);
        void emitFilterAddedNotification();

        Wt::WContainerWidget* _filters{};
        Wt::WInteractWidget* _mediaLibraryFilter{};
        Wt::WInteractWidget* _labelFilter{};
        Wt::WInteractWidget* _releaseTypeFilter{};

        Wt::Signal<> _sigUpdated;

        db::Filters _dbFilters;
    };
} // namespace lms::ui
