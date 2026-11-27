/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <exception>
#include <string>

#include "core/Service.hpp"

#include "database/profiling/IQueryProfiler.hpp"

namespace Wt::Dbo
{
    class Session;
}

namespace lms::db::utils
{
    class ScopedQueryProfiler
    {
    public:
        template<typename Query>
        explicit ScopedQueryProfiler(const Query& query)
            : _recorder{ core::Service<IQueryProfiler>::get() }
        {
            if (_recorder)
            {
                _session = &query.session();
                _query = query.asString();
                _start = IQueryProfiler::Clock::now();
            }
        }

        ~ScopedQueryProfiler();

        ScopedQueryProfiler(const ScopedQueryProfiler&) = delete;
        ScopedQueryProfiler& operator=(const ScopedQueryProfiler&) = delete;

        void suspend();
        void resume();

    private:
        IQueryProfiler* _recorder{};
        const int _uncaughtExceptionCount{ std::uncaught_exceptions() };
        Wt::Dbo::Session* _session{};
        std::string _query;
        IQueryProfiler::Clock::time_point _start;
        IQueryProfiler::Clock::duration _elapsed{};
        bool _active{ true };
    };
} // namespace lms::db::utils
