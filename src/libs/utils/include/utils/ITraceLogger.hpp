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

#define LMS_SUPPORT_TRACING 1

#define LMS_CONCAT_IMPL(x, y) x##y
#define LMS_CONCAT(x, y) LMS_CONCAT_IMPL(x, y)

#if LMS_SUPPORT_TRACING 
#define LMS_SCOPED_TRACE(CATEGORY, LEVEL, NAME) ::tracing::ScopedTrace LMS_CONCAT(ScopedTrace_, __LINE__){ CATEGORY, LEVEL, NAME }

#else
#define LMS_SCOPED_TRACE(CATEGORY, LEVEL, NAME)      (void)0
#endif

#define LMS_SCOPED_TRACE_OVERVIEW(CATEGORY, NAME)   LMS_SCOPED_TRACE(CATEGORY, ::tracing::Level::Overview, NAME)
#define LMS_SCOPED_TRACE_DETAILED(CATEGORY, NAME)   LMS_SCOPED_TRACE(CATEGORY, ::tracing::Level::Detailed, NAME)

namespace tracing
{
    using clock = std::chrono::steady_clock;

    enum class Level
    {
        Overview,
        Detailed,
    };

    class ITraceLogger
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

        virtual ~ITraceLogger() = default;

        virtual bool isLevelActive(Level level) const = 0;
        virtual void write(const CompleteEvent& entry) = 0;
        virtual void dumpCurrentBuffer(std::ostream& os) = 0;
        virtual void setThreadName(std::thread::id id, std::string_view threadName) = 0;
    };

    static constexpr std::size_t MinBufferSizeInMBytes = 16;
    std::unique_ptr<ITraceLogger> createTraceLogger(Level minLevel = Level::Overview, std::size_t bufferSizeInMbytes = MinBufferSizeInMBytes);

    class ScopedTrace
    {
    public:
        ScopedTrace(LiteralString category, Level level, LiteralString name, ITraceLogger* traceLogger = Service<ITraceLogger>::get())
        {
            if (traceLogger && traceLogger->isLevelActive(level))
            {
                _traceLogger = traceLogger;

                _event.start = clock::now();
                _event.threadId = std::this_thread::get_id();
                _event.name = name;
                _event.category = category;
            }
            else
            {
                _traceLogger = nullptr;
            }
        }

        ~ScopedTrace()
        {
            if (_traceLogger)
            {
                _event.duration = clock::now() - _event.start;
                _traceLogger->write(_event);
            }
        }

    private:
        ScopedTrace(const ScopedTrace&) = delete;
        ScopedTrace& operator=(const ScopedTrace&) = delete;

        ITraceLogger* _traceLogger;
        ITraceLogger::CompleteEvent _event;
    };
}