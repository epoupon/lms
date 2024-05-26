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

#include <array>
#include <deque>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/ITraceLogger.hpp"

namespace lms::core::tracing
{
    class TraceLogger : public ITraceLogger
    {
    public:
        TraceLogger(Level minLevel, std::size_t bufferSizeinMBytes);

        void onThreadPreDestroy();

    private:
        bool isLevelActive(Level level) const override;
        void write(const CompleteEvent& event) override;
        void dumpCurrentBuffer(std::ostream& os) override;
        void setThreadName(std::thread::id id, std::string_view threadName) override;
        ArgHashType registerArg(LiteralString argType, std::string_view argValue) override;
        void setMetadata(std::string_view metadata, std::string_view value) override;

        std::size_t getRegisteredArgCount() const;

        static ArgHashType computeArgHash(LiteralString type, std::string_view value);
        static std::uint32_t toTraceThreadId(std::thread::id threadId);

        static constexpr std::size_t BufferSize{ 64 * 1024 };

        // Same as ComplteEvent, but compacted
        struct CompleteEventEntry
        {
            clock::time_point start;
            clock::duration duration;
            const char* name;
            const char* category;
            ArgHashType arg;
        };
        static constexpr ArgHashType invalidHash{ 0 };

        struct alignas(64) Buffer
        {
            static constexpr std::size_t CompleteEventCount{ BufferSize / sizeof(CompleteEventEntry) };

            std::thread::id threadId;
            std::array<CompleteEventEntry, CompleteEventCount> durationEvents;
            std::atomic<std::size_t> currentDurationIndex{};
        };

        Buffer* acquireBuffer();
        void releaseBuffer(Buffer* buffer);

        const Level _minLevel;
        const clock::time_point _start;
        const std::thread::id _creatorThreadId;

        std::vector<Buffer> _buffers; // allocated once during construction

        mutable std::shared_mutex _argMutex;
        struct ArgEntry
        {
            LiteralString type;
            std::string value;
        };
        using ArgEntryMap = std::unordered_map<ArgHashType, ArgEntry>;
        ArgEntryMap _argEntries; // collisions not handled

        std::mutex _threadNameMutex;
        std::unordered_map<std::thread::id, std::string> _threadNames;

        std::mutex _metadataMutex;
        std::map<std::string, std::string> _metadata;

        std::mutex _mutex;
        std::deque<Buffer*> _freeBuffers;

        static thread_local Buffer* _currentBuffer;
    };
} // namespace lms::core::tracing