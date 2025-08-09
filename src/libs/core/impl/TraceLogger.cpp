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
#include "core/String.hpp"

namespace lms::core::tracing
{
    namespace
    {
        class CurrentThreadUnregisterer
        {
        public:
            CurrentThreadUnregisterer(TraceLogger* logger)
                : _logger{ logger } {}
            ~CurrentThreadUnregisterer()
            {
                if (_logger)
                    _logger->onThreadPreDestroy();
            }

        private:
            CurrentThreadUnregisterer(const CurrentThreadUnregisterer&) = delete;
            CurrentThreadUnregisterer& operator=(const CurrentThreadUnregisterer&) = delete;
            CurrentThreadUnregisterer(CurrentThreadUnregisterer&&) = delete;
            CurrentThreadUnregisterer& operator=(CurrentThreadUnregisterer&&) = delete;

            TraceLogger* _logger;
        };
    } // namespace

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

        LMS_LOG(UTILS, INFO, "TraceLogger: using " << _buffers.size() << " buffers. Buffer size = " << std::to_string(BufferSize) << ", entry size = " << sizeof(CompleteEventEntry) << ", entry count per buffer = " << Buffer::CompleteEventCount);

        setMetadata("cpu_count", std::to_string(std::thread::hardware_concurrency()));
        setMetadata("build_type",
#ifndef NDEBUG
            "debug"
#else
            "release"
#endif
        );
    }

    bool TraceLogger::isLevelActive(Level level) const
    {
        return static_cast<std::underlying_type_t<Level>>(level) <= static_cast<std::underlying_type_t<Level>>(_minLevel);
    }

    void TraceLogger::write(const CompleteEvent& event)
    {
        if (!_currentBuffer)
            _currentBuffer = acquireBuffer();

        CompleteEventEntry& entry{ _currentBuffer->durationEvents[_currentBuffer->currentDurationIndex] };
        entry.start = event.start;
        entry.duration = event.duration;
        entry.name = event.name.c_str();
        entry.category = event.category.c_str();
        entry.arg = event.arg.value_or(invalidHash);

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
        buffer->threadId = std::this_thread::get_id();
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
                os << "\"tid\" : " << toTraceThreadId(threadId) << ", ";
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
                const auto threadId{ toTraceThreadId(buffer.threadId) };

                for (std::size_t i{}; i < buffer.currentDurationIndex; ++i)
                {
                    // Looks like tracing viewer is not pleased when nested event start at the same timestamp
                    // Hence the double representation as the microsecond unit is not precise enough
                    using clockMicro = std::chrono::duration<double, std::micro>;

                    const CompleteEventEntry& event{ buffer.durationEvents[i] };

                    if (first)
                        first = false;
                    else
                        os << ", " << std::endl;
                    ;

                    os << "\t\t{ ";
                    os << "\"name\" : \"" << event.name << "\", ";
                    os << "\"cat\" : \"" << event.category << "\", ";
                    os << "\"pid\": 1, ";
                    os << "\"tid\" : " << threadId << ", ";
                    os << "\"ts\" : " << std::fixed << std::setprecision(3) << std::chrono::duration_cast<clockMicro>(event.start - _start).count() << ", ";
                    os << "\"dur\" : " << std::fixed << std::setprecision(3) << std::chrono::duration_cast<clockMicro>(event.duration).count() << ", ";
                    os << "\"ph\" : \"X\"";
                    if (event.arg != invalidHash)
                    {
                        ArgEntryMap::const_iterator itArgEntry;
                        {
                            std::shared_lock lock{ _argMutex };
                            itArgEntry = _argEntries.find(event.arg);
                            assert(itArgEntry != _argEntries.cend());
                        }

                        os << ", \"args\" : { \"" << itArgEntry->second.type.c_str() << "\" : \"";
                        stringUtils::writeJsonEscapedString(os, std::string_view{ itArgEntry->second.value });
                        os << "\" }";
                    }
                    os << " }";
                }
            }
        }

        os << std::endl;
        os << "\t]," << std::endl;
        {
            std::scoped_lock lock{ _metadataMutex };
            for (const auto& [metadata, value] : _metadata)
            {
                os << "\t\"meta_" << metadata << "\": \"";
                stringUtils::writeJsonEscapedString(os, value);
                os << "\"," << std::endl;
            }
        }
        os << "\t\"meta_registered_arg_count\" : " << getRegisteredArgCount() << std::endl;
        os << "}" << std::endl;
    }

    void TraceLogger::setThreadName(std::thread::id id, std::string_view threadName)
    {
        std::scoped_lock lock{ _threadNameMutex };
        _threadNames.emplace(id, threadName);
    }

    TraceLogger::ArgHashType TraceLogger::computeArgHash(LiteralString type, std::string_view value)
    {
        ArgHashType res{};
        res ^= std::hash<std::string_view>{}(type.str());
        res ^= std::hash<std::string_view>{}(value);
        return res;
    }

    TraceLogger::ArgHashType TraceLogger::registerArg(LiteralString argType, std::string_view argValue)
    {
        const ArgHashType hash{ computeArgHash(argType, argValue) };
        assert(hash != invalidHash);

        {
            const std::shared_lock lock{ _argMutex };

            auto itArgEntry{ _argEntries.find(hash) };
            if (itArgEntry != std::cend(_argEntries))
            {
                assert(itArgEntry->second.type == argType);
                assert(itArgEntry->second.value == argValue);
                return hash;
            }
        }

        {
            const std::unique_lock lock{ _argMutex };

            auto itArgEntry{ _argEntries.find(hash) };
            if (itArgEntry != std::cend(_argEntries))
            {
                assert(itArgEntry->second.type == argType);
                assert(itArgEntry->second.value == argValue);
                return hash;
            }

            _argEntries.emplace(hash, ArgEntry{ argType, std::string{ argValue } });
            return hash;
        }
    }

    void TraceLogger::setMetadata(std::string_view metadata, std::string_view value)
    {
        const std::scoped_lock lock{ _metadataMutex };
        _metadata[std::string{ metadata }] = value;
    }

    std::size_t TraceLogger::getRegisteredArgCount() const
    {
        const std::shared_lock lock{ _argMutex };

        return _argEntries.size();
    }

    std::uint32_t TraceLogger::toTraceThreadId(std::thread::id threadId)
    {
        // Pefetto UI does not accept 64bits thread ids
        std::ostringstream oss;
        oss << threadId;

        std::istringstream iss{ oss.str() };
        std::uint64_t id{};
        iss >> id;

        return static_cast<std::uint32_t>(id);
    }
} // namespace lms::core::tracing