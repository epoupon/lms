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

#include "TraceLogger.hpp"

#include <iomanip>
#include <memory>
#include <string>
#include "core/Exception.hpp"
#include "core/ILogger.hpp"

namespace lms::core::tracing
{
    namespace
    {
        class CurrentThreadUnregisterer
        {
        public:
            CurrentThreadUnregisterer(TraceLogger* logger) : _logger{ logger } {}
            ~CurrentThreadUnregisterer()
            {
                if (_logger)
                    _logger->onThreadPreDestroy();
            }

        private:
            CurrentThreadUnregisterer(const CurrentThreadUnregisterer&) = delete;
            CurrentThreadUnregisterer& operator=(const CurrentThreadUnregisterer&) = delete;

            TraceLogger* _logger;
        };
    }

    thread_local TraceLogger::Buffer* TraceLogger::_currentBuffer{};

    std::unique_ptr<ITraceLogger> createTraceLogger(Level minLevel, std::size_t bufferSizeInMbytes)
    {
        return std::make_unique<TraceLogger>(minLevel, bufferSizeInMbytes);
    }

    TraceLogger::TraceLogger(Level minLevel, std::size_t bufferSizeinMBytes)
        : _minLevel{ minLevel }
        , _start{ clock::now() }
        , _creatorThreadId{ std::this_thread::get_id() }
        , _buffers((bufferSizeinMBytes * 1024 * 1024) / BufferSize)
    {
        if (bufferSizeinMBytes < MinBufferSizeInMBytes)
            throw LmsException{ "TraceLogger must be configured with at least " + std::to_string(MinBufferSizeInMBytes) + " MBytes" };

        setThreadName(_creatorThreadId, "MainThread");

        for (Buffer& buffer : _buffers)
            _freeBuffers.push_back(&buffer);

        LMS_LOG(UTILS, INFO, "TraceLogger: using " << _buffers.size() << " buffers. Buffer size = " << std::to_string(BufferSize));
    }

    bool TraceLogger::isLevelActive(Level level) const
    {
        return static_cast<std::underlying_type_t<Level>>(level) <= static_cast<std::underlying_type_t<Level>>(_minLevel);
    }

    void TraceLogger::write(const CompleteEvent& event)
    {
        if (!_currentBuffer)
            _currentBuffer = acquireBuffer();

        _currentBuffer->durationEvents[_currentBuffer->currentDurationIndex] = event;

        // update the index after writing the event, in case another thread wants to dump
        if (++_currentBuffer->currentDurationIndex == _currentBuffer->durationEvents.size())
        {
            releaseBuffer(_currentBuffer);
            _currentBuffer = nullptr;
        }
    }

    void TraceLogger::onThreadPreDestroy()
    {
        if (_currentBuffer)
            releaseBuffer(_currentBuffer);
    }

    TraceLogger::Buffer* TraceLogger::acquireBuffer()
    {
        // We consider the creator thread will survive the trace logger (thus we don't want to release anything on thread destruction)
        static thread_local CurrentThreadUnregisterer currentThreadUnregister{ _creatorThreadId == std::this_thread::get_id() ? nullptr : this };

        std::scoped_lock lock{ _mutex };
        assert(!_freeBuffers.empty());

        TraceLogger::Buffer* buffer{ _freeBuffers.front() };
        _freeBuffers.pop_front();

        // Empty new buffer only now (we want to keep history on released buffers since we dump them)
        buffer->currentDurationIndex = 0;
        return buffer;
    }

    void TraceLogger::releaseBuffer(Buffer* buffer)
    {
        assert(buffer);

        std::scoped_lock lock{ _mutex };
        _freeBuffers.push_back(buffer);
    }

    void TraceLogger::dumpCurrentBuffer(std::ostream& os)
    {
        os << "{" << std::endl;
        os << "\t\"traceEvents\": [" << std::endl;

        bool first{ true };

        {
            std::scoped_lock lock{ _threadNameMutex };

            for (const auto& [threadId, threadName] : _threadNames)
            {
                if (first)
                    first = false;
                else
                    os << ", " << std::endl;

                os << "\t\t{ ";
                os << "\"name\" : \"thread_name\", ";
                os << "\"pid\" : 1, ";
                os << "\"tid\" : " << threadId << ", ";
                os << "\"ph\" : \"M\", ";
                os << "\"args\" : { \"name\" : \"" + threadName + "\" }";
                os << " }";
            }
        }

        // we allow threads to fill in their current block while dumping
        {
            std::scoped_lock lock{ _mutex };

            for (Buffer& buffer : _buffers)
            {
                for (std::size_t i{}; i < buffer.currentDurationIndex; ++i)
                {
                    // Looks like tracing viewer is not pleased when nested event start at the same timestamp
                    // Hence the double representation as the microsecond unit is not precise enough
                    using clockMicro = std::chrono::duration<double, std::micro>;

                    const CompleteEvent& event{ buffer.durationEvents[i] };

                    if (first)
                        first = false;
                    else
                        os << ", " << std::endl;;

                    os << "\t\t{ ";
                    os << "\"name\" : \"" << event.name.c_str() << "\", ";
                    os << "\"cat\" : \"" << event.category.c_str() << "\", ";
                    os << "\"pid\": 1, ";
                    os << "\"tid\" : " << event.threadId << ", ";
                    os << "\"ts\" : " << std::fixed << std::setprecision(3) << std::chrono::duration_cast<clockMicro>(event.start - _start).count() << ", ";
                    os << "\"dur\" : " << std::fixed << std::setprecision(3) << std::chrono::duration_cast<clockMicro>(event.duration).count() << ", ";
                    os << "\"ph\" : \"X\"";
                    os << " }";
                }
            }
        }

        os << std::endl;
        os << "\t]," << std::endl;
        os << "\t\"meta_cpu_count\" : " << std::thread::hardware_concurrency() << ", " << std::endl;
        os << "\t\"meta_build_type\" : ";
#ifndef NDEBUG
        os << "\"debug\"";
#else
        os << "\"release\"";
#endif
        os << std::endl;
        os << "}" << std::endl;
    }

    void TraceLogger::setThreadName(std::thread::id id, std::string_view threadName)
    {
        std::scoped_lock lock{ _threadNameMutex };
        _threadNames.emplace(id, threadName);
    }
}