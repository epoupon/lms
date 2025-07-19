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

#include <Wt/WContainerWidget.h>
#include <Wt/WTemplate.h>

#include "database/objects/MediaLibraryId.hpp"

namespace lms::ui
{
    class MediaLibrariesView : public Wt::WTemplate
    {
    public:
        MediaLibrariesView();

    private:
        void refreshView();
        void showDeleteLibraryModal(db::MediaLibraryId library, Wt::WTemplate* libraryEntry);
        void updateEntry(db::MediaLibraryId library, Wt::WTemplate* libraryEntry);
        Wt::WTemplate* addEntry();

        Wt::WContainerWidget* _libraries{};
    };
} // namespace lms::ui