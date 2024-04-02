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
#include <optional>
#include <ostream>
#include <string_view>
#include <thread>

#include "LiteralString.hpp"
#include "Service.hpp"

#define LMS_SUPPORT_TRACING 1

#define LMS_CONCAT_IMPL(x, y) x##y
#define LMS_CONCAT(x, y) LMS_CONCAT_IMPL(x, y)

#if LMS_SUPPORT_TRACING 
#define LMS_SCOPED_TRACE(CATEGORY, LEVEL, NAME, ARGTYPE, ARGVALUE) \
std::optional<::lms::core::tracing::ScopedTrace> LMS_CONCAT(ScopedTrace_, __LINE__); \
if (::lms::core::tracing::ITraceLogger* traceLogger{ ::lms::core::Service<::lms::core::tracing::ITraceLogger>::get() }; traceLogger && traceLogger->isLevelActive(LEVEL)) \
    LMS_CONCAT(ScopedTrace_, __LINE__).emplace(CATEGORY, LEVEL, NAME, ARGTYPE, ARGVALUE, traceLogger);
#else
#define LMS_SCOPED_TRACE(CATEGORY, LEVEL, NAME)      (void)0
#endif

#define LMS_SCOPED_TRACE_OVERVIEW_WITH_ARG(CATEGORY, NAME, ARGTYPE, ARGVALUE)   LMS_SCOPED_TRACE(CATEGORY, ::lms::core::tracing::Level::Overview, NAME, ARGTYPE, ARGVALUE)
#define LMS_SCOPED_TRACE_DETAILED_WITH_ARG(CATEGORY, NAME, ARGTYPE, ARGVALUE)   LMS_SCOPED_TRACE(CATEGORY, ::lms::core::tracing::Level::Detailed, NAME, ARGTYPE, ARGVALUE)

#define LMS_SCOPED_TRACE_OVERVIEW(CATEGORY, NAME)   LMS_SCOPED_TRACE_OVERVIEW_WITH_ARG(CATEGORY, NAME, "", "")
#define LMS_SCOPED_TRACE_DETAILED(CATEGORY, NAME)   LMS_SCOPED_TRACE_DETAILED_WITH_ARG(CATEGORY, NAME, "", "")

namespace lms::core::tracing
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
        using ArgHashType = std::size_t;

        struct CompleteEvent
        {
            clock::time_point start;
            clock::duration duration;
            LiteralString name;
            LiteralString category;
            std::optional<ArgHashType> arg;
        };

        virtual ~ITraceLogger() = default;

        virtual bool isLevelActive(Level level) const = 0;
        virtual void write(const CompleteEvent& entry) = 0;
        virtual void dumpCurrentBuffer(std::ostream& os) = 0;
        virtual void setThreadName(std::thread::id id, std::string_view threadName) = 0;
        virtual void setMetadata(std::string_view metadata, std::string_view value) = 0;

        virtual ArgHashType registerArg(LiteralString argType, std::string_view argValue) = 0;
    };

    static constexpr std::size_t MinBufferSizeInMBytes = 16;
    std::unique_ptr<ITraceLogger> createTraceLogger(Level minLevel = Level::Overview, std::size_t bufferSizeInMbytes = MinBufferSizeInMBytes);

    class ScopedTrace
    {
    public:
        ScopedTrace(LiteralString category, Level level, LiteralString name, LiteralString argType = {}, std::string_view argValue = {}, ITraceLogger* traceLogger = Service<ITraceLogger>::get())
        {
            if (traceLogger && traceLogger->isLevelActive(level))
            {
                _traceLogger = traceLogger;

                _event.start = clock::now();
                _event.name = name;
                _event.category = category;
                if (!argType.empty() && !argValue.empty())
                    _event.arg = traceLogger->registerArg(argType, argValue);
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