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
#include <mutex>
#include <vector>
#include <thread>
#include <unordered_map>

#include "utils/ITraceLogger.hpp"

namespace tracing
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

        static constexpr std::size_t BufferSize{ 32 * 1024 };

        struct alignas(64) Buffer
        {
            static constexpr std::size_t CompleteEventCount{ BufferSize / sizeof(CompleteEvent) };

            std::array<CompleteEvent, CompleteEventCount> durationEvents;
            std::atomic<std::size_t> currentDurationIndex{};
        };

        Buffer* acquireBuffer();
        void releaseBuffer(Buffer* buffer);

        const Level _minLevel;
        const clock::time_point _start;
        const std::thread::id _creatorThreadId;

        std::vector<Buffer> _buffers; // allocated once during construction

        std::mutex _threadNameMutex;
        std::unordered_map<std::thread::id, std::string> _threadNames;

        std::mutex _mutex;
        std::deque<Buffer*> _freeBuffers;

        static thread_local Buffer* _currentBuffer;
    };
}