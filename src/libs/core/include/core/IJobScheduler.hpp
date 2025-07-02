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
#include <vector>

#include "core/LiteralString.hpp"

namespace lms::core
{
    class IJob;
    class IJobScheduler
    {
    public:
        virtual ~IJobScheduler() = default;

        using ShouldAbortCallback = std::function<bool()>;
        virtual void setShouldAbortCallback(ShouldAbortCallback callback) = 0;

        virtual std::size_t getThreadCount() const = 0;
        virtual void scheduleJob(std::unique_ptr<IJob> job) = 0;

        virtual std::size_t getJobsDoneCount() const = 0;
        virtual size_t popJobsDone(std::vector<std::unique_ptr<IJob>>& jobs, std::size_t maxCount) = 0;

        virtual void waitUntilJobCountAtMost(std::size_t maxOngoingJobs) = 0;
        virtual void wait() = 0;
    };

    std::unique_ptr<IJobScheduler> createJobScheduler(core::LiteralString name, std::size_t threadCount);
} // namespace lms::core