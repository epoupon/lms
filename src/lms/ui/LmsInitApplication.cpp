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

#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>

#include "LmsInitApplication.hpp"

namespace lms::ui
{
    LmsInitApplication::LmsInitApplication(const Wt::WEnvironment& env)
        : Wt::WApplication{ env }
    {
        enableUpdates(true);
        root()->addNew<Wt::WText>("LMS is initializing. This may take a while, please wait...");
    }

    std::unique_ptr<Wt::WApplication> LmsInitApplication::create(const Wt::WEnvironment& env)
    {
        return std::make_unique<LmsInitApplication>(env);
    }
} // namespace lms::ui