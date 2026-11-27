/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <cassert>
#include <exception>

#include "core/ILogger.hpp"

#include "profiling/QueryProfiler.hpp"
#include "profiling/ScopedQueryProfiler.hpp"

namespace lms::db::utils
{
    ScopedQueryProfiler::~ScopedQueryProfiler()
    {
        if (!_recorder || std::uncaught_exceptions() != _uncaughtExceptionCount)
            return;

        if (_active)
            _elapsed += IQueryProfiler::Clock::now() - _start;

        try
        {
            static_cast<QueryProfiler*>(_recorder)->recordQueryExecution(*_session, _query, _elapsed);
        }
        catch (const std::exception& e)
        {
            LMS_LOG(DB, ERROR, "Cannot record query execution: " << e.what());
        }
    }

    void ScopedQueryProfiler::suspend()
    {
        if (_recorder)
        {
            assert(_active);
            _elapsed += IQueryProfiler::Clock::now() - _start;
            _active = false;
        }
    }

    void ScopedQueryProfiler::resume()
    {
        if (_recorder)
        {
            assert(!_active);
            _start = IQueryProfiler::Clock::now();
            _active = true;
        }
    }
} // namespace lms::db::utils
