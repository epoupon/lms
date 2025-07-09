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

#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <Wt/WSignal.h>

#include "database/objects/UserId.hpp"

namespace lms::ui
{
    class LmsApplication;
    class LmsApplicationManager
    {
    public:
        Wt::Signal<LmsApplication&> applicationRegistered;
        Wt::Signal<LmsApplication&> applicationUnregistered;

    private:
        friend class LmsApplication;

        void registerApplication(LmsApplication& application);
        void unregisterApplication(LmsApplication& application);

        std::mutex _mutex;
        std::unordered_map<db::UserId, std::unordered_set<LmsApplication*>> m_applications;
    };
} // namespace lms::ui
