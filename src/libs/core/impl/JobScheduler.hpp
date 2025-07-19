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

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

#include "core/IJobScheduler.hpp"
#include "core/IOContextRunner.hpp"

namespace lms::core
{
    class JobScheduler : public IJobScheduler
    {
    public:
        JobScheduler(core::LiteralString name, std::size_t threadCount);
        ~JobScheduler() override;
        JobScheduler(const JobScheduler&) = delete;
        JobScheduler& operator=(const JobScheduler&) = delete;

    private:
        void setShouldAbortCallback(ShouldAbortCallback callback) override;

        std::size_t getThreadCount() const override;
        void scheduleJob(std::unique_ptr<IJob> job) override;

        std::size_t getJobsDoneCount() const override;
        size_t popJobsDone(std::vector<std::unique_ptr<IJob>>& jobs, std::size_t maxCount) override;

        void waitUntilJobCountAtMost(std::size_t maxOngoingJobs) override;
        void wait() override;

        core::LiteralString _name;
        boost::asio::io_context _ioContext;
        core::IOContextRunner _ioContextRunner;

        ShouldAbortCallback _abortCallback;

        mutable std::mutex _mutex;
        std::atomic<std::size_t> _ongoingJobCount;
        std::deque<std::unique_ptr<IJob>> _doneJobs;
        std::condition_variable _condVar;
    };
} // namespace lms::core