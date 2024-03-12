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

#include <vector>
#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>
#include <Wt/WTemplate.h>

#include "database/ClusterId.hpp"

#include "Filters.hpp"

namespace lms::ui
{
    class Filters : public Wt::WTemplate
    {
    public:
        Filters();

        const std::vector<db::ClusterId>& getClusterIds() const { return _clusterIds; }
        void add(db::ClusterId clusterId);

        Wt::Signal<>& updated() { return _sigUpdated; }

    private:
        void showDialog();

        Wt::WContainerWidget* _filters;
        Wt::Signal<> _sigUpdated;
        std::vector<db::ClusterId> _clusterIds;
    };
} // namespace lms::ui

