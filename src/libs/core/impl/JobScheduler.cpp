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

#include "JobScheduler.hpp"

#include <boost/asio/post.hpp>

#include "core/ITraceLogger.hpp"

#include "core/IJob.hpp"

namespace lms::core
{
    std::unique_ptr<IJobScheduler> createJobScheduler(core::LiteralString name, std::size_t threadCount)
    {
        return std::make_unique<JobScheduler>(name, threadCount);
    }

    JobScheduler::JobScheduler(core::LiteralString name, std::size_t threadCount)
        : _name{ name }
        , _ioContextRunner{ _ioContext, threadCount, name.str() }
    {
    }

    JobScheduler::~JobScheduler() = default;

    void JobScheduler::setShouldAbortCallback(ShouldAbortCallback callback)
    {
        _abortCallback = callback;
    }

    std::size_t JobScheduler::getThreadCount() const
    {
        return _ioContextRunner.getThreadCount();
    }

    void JobScheduler::scheduleJob(std::unique_ptr<IJob> job)
    {
        {
            std::scoped_lock lock{ _mutex };
            _ongoingJobCount += 1;
        }

        auto jobHandler{ [job = std::move(job), this]() mutable {
            if (_abortCallback && _abortCallback())
            {
                std::scoped_lock lock{ _mutex };
                _ongoingJobCount -= 1;
            }
            else
            {
                {
                    LMS_SCOPED_TRACE_OVERVIEW(_name, job->getName());
                    job->run();
                }

                {
                    std::scoped_lock lock{ _mutex };

                    _doneJobs.emplace_back(std::move(job));
                    _ongoingJobCount -= 1;
                }
            }

            _condVar.notify_all();
        } };

        boost::asio::post(_ioContext, std::move(jobHandler));
    }

    std::size_t JobScheduler::getJobsDoneCount() const
    {
        std::scoped_lock lock{ _mutex };
        return _doneJobs.size();
    }

    size_t JobScheduler::popJobsDone(std::vector<std::unique_ptr<IJob>>& doneJobs, std::size_t maxCount)
    {
        doneJobs.clear();
        doneJobs.reserve(maxCount);

        {
            std::scoped_lock lock{ _mutex };

            while (doneJobs.size() < maxCount && !_doneJobs.empty())
            {
                doneJobs.push_back(std::move(_doneJobs.front()));
                _doneJobs.pop_front();
            }
        }

        return doneJobs.size();
    }

    void JobScheduler::waitUntilJobCountAtMost(std::size_t maxOngoingJobs)
    {
        if (_ongoingJobCount <= maxOngoingJobs)
            return;

        {
            LMS_SCOPED_TRACE_OVERVIEW(_name, "WaitJobs");

            std::unique_lock lock{ _mutex };
            _condVar.wait(lock, [=, this] { return _ongoingJobCount <= maxOngoingJobs; });
        }
    }

    void JobScheduler::wait()
    {
        waitUntilJobCountAtMost(0);
    }

} // namespace lms::core
