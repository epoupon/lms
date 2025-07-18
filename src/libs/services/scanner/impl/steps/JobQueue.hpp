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

#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace lms::core
{
    class IJob;
    class IJobScheduler;
} // namespace lms::core

namespace lms::scanner
{
    class JobQueue
    {
    public:
        using ProcessFunction = std::function<void(std::span<std::unique_ptr<core::IJob>>)>;

        // processBatchSize -> how many jobs done to notify at once using processJobsDoneFunc
        // drainThreshold: fraction of maxQueueSize at which completed jobs are processed
        JobQueue(core::IJobScheduler& scheduler, std::size_t maxQueueSize, ProcessFunction processJobsDoneFunc, std::size_t processBatchSize, float drainThreshold);
        ~JobQueue();
        JobQueue(const JobQueue&) = delete;
        JobQueue& operator=(const JobQueue&) = delete;

        // push can wait and invoke the supplied ProcessFunction
        void push(std::unique_ptr<core::IJob> job);
        void finish();

    private:
        void drainIfNeeded();

        core::IJobScheduler& _scheduler;
        const std::size_t _maxQueueSize;
        ProcessFunction _processJobsDoneFunc;
        const std::size_t _batchSize;
        const float _drainThreshold;
        std::vector<std::unique_ptr<core::IJob>> _jobsDone;
    };

} // namespace lms::scanner