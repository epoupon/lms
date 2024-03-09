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

#include <chrono>
#include <memory>
#include <ostream>
#include <string_view>
#include <thread>

#include "LiteralString.hpp"
#include "Service.hpp"

#define LMS_ENABLE_PROFILING 1

#define LMS_CONCAT_IMPL(x, y) x##y
#define LMS_CONCAT(x, y) LMS_CONCAT_IMPL(x, y)

#if LMS_ENABLE_PROFILING 
#define LMS_SCOPED_PROFILE(CATEGORY, LEVEL, NAME) ::profiling::ScopedEvent LMS_CONCAT(scopedEvent_, __LINE__){ CATEGORY, LEVEL, NAME }

#else
#define LMS_SCOPED_PROFILE(CATEGORY, LEVEL, NAME)      (void)0
#endif

#define LMS_SCOPED_PROFILE_OVERVIEW(CATEGORY, NAME)   LMS_SCOPED_PROFILE(CATEGORY, ::profiling::Level::Overview, NAME)
#define LMS_SCOPED_PROFILE_DETAILED(CATEGORY, NAME)   LMS_SCOPED_PROFILE(CATEGORY, ::profiling::Level::Detailed, NAME)

namespace profiling
{
    using clock = std::chrono::steady_clock;

    enum class Level
    {
        Overview,
        Detailed,
    };

    class IProfiler
    {
    public:
        struct CompleteEvent
        {
            clock::time_point start;
            clock::duration duration;
            std::thread::id threadId;
            LiteralString name;
            LiteralString category;
        };

        virtual ~IProfiler() = default;

        virtual bool isLevelActive(Level level) const = 0;
        virtual void write(const CompleteEvent& entry) = 0;
        virtual void dumpCurrentBuffer(std::ostream& os) = 0;
        virtual void setThreadName(std::thread::id id, std::string_view threadName) = 0;
    };

    static constexpr std::size_t MinBufferSizeInMBytes = 16;
    std::unique_ptr<IProfiler> createProfiler(Level minLevel = Level::Overview, std::size_t bufferSizeInMbytes = MinBufferSizeInMBytes);

    class ScopedEvent
    {
    public:
        ScopedEvent(LiteralString category, Level level, LiteralString name, IProfiler* profiler = Service<IProfiler>::get())
        {
            if (profiler && profiler->isLevelActive(level))
            {
                _profiler = profiler;

                _event.start = clock::now();
                _event.threadId = std::this_thread::get_id();
                _event.name = name;
                _event.category = category;
            }
            else
            {
                _profiler = nullptr;
            }
        }

        ~ScopedEvent()
        {
            if (_profiler)
            {
                _event.duration = clock::now() - _event.start;
                _profiler->write(_event);
            }
        }

    private:
        ScopedEvent(const ScopedEvent&) = delete;
        ScopedEvent& operator=(const ScopedEvent&) = delete;

        IProfiler* _profiler;
        IProfiler::CompleteEvent _event;
    };
}