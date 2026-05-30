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

#include <cassert>
#include <string>

#include "core/Service.hpp"
#include "database/profiling/IQueryProfiler.hpp"

#include "profiling/QueryProfiler.hpp"

namespace lms::db::utils
{
    template<typename Query>
    class ScopedQueryProfiler
    {
    public:
        explicit ScopedQueryProfiler(const Query& query)
            : _recorder{ static_cast<QueryProfiler*>(core::Service<IQueryProfiler>::get()) }
        {
            if (_recorder)
            {
                _query = &query;
                _start = IQueryProfiler::Clock::now();
            }
        }

        ~ScopedQueryProfiler()
        {
            if (_recorder)
            {
                if (_active)
                    _elapsed += IQueryProfiler::Clock::now() - _start;
                _recorder->recordQueryExecution(_query->session(), _query->asString(), _elapsed);
            }
        }

        ScopedQueryProfiler(const ScopedQueryProfiler&) = delete;
        ScopedQueryProfiler& operator=(const ScopedQueryProfiler&) = delete;

        void suspend()
        {
            if (_recorder)
            {
                assert(_active);
                _elapsed += IQueryProfiler::Clock::now() - _start;
                _active = false;
            }
        }

        void resume()
        {
            if (_recorder)
            {
                assert(!_active);
                _start = IQueryProfiler::Clock::now();
                _active = true;
            }
        }

    private:
        QueryProfiler* _recorder{};
        const Query* _query{};
        IQueryProfiler::Clock::time_point _start;
        IQueryProfiler::Clock::duration _elapsed{};
        bool _active{ true };
    };
} // namespace lms::db::utils
