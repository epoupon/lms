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

#include <gtest/gtest.h>

#include <thread>

#include "core/IJob.hpp"
#include "core/IJobScheduler.hpp"

namespace lms::core
{
    namespace
    {
        class TestJob : public IJob
        {
        public:
            TestJob(std::atomic<std::size_t>& count)
                : workCount(count) {}

        private:
            LiteralString getName() const override { return "test"; };
            void run() override
            {
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++workCount;
            }

            std::atomic<std::size_t>& workCount;
        };

    } // namespace
    TEST(JobScheduler, basic)
    {
        std::atomic<std::size_t> workCount{ 0 };

        auto scheduler{ createJobScheduler("TestScheduler", 2) };
        ASSERT_NE(scheduler, nullptr);

        for (int i = 0; i < 10; ++i)
            scheduler->scheduleJob(std::make_unique<TestJob>(workCount));

        // Wait for all jobs to complete
        scheduler->wait();
        EXPECT_EQ(workCount.load(), 10);

        std::vector<std::unique_ptr<IJob>> doneJobs;
        scheduler->popJobsDone(doneJobs, 10);
        EXPECT_EQ(doneJobs.size(), 10);
    }

    TEST(JobScheduler, abort)
    {
        std::atomic<std::size_t> workCount{ 0 };

        auto scheduler{ createJobScheduler("TestScheduler", 2) };
        ASSERT_NE(scheduler, nullptr);

        scheduler->setShouldAbortCallback([] { return true; });

        for (int i = 0; i < 10; ++i)
            scheduler->scheduleJob(std::make_unique<TestJob>(workCount));

        // Wait for all jobs to complete
        scheduler->wait();
        EXPECT_EQ(workCount.load(), 0); // nothing was done

        std::vector<std::unique_ptr<IJob>> doneJobs;
        scheduler->popJobsDone(doneJobs, 10);
        EXPECT_EQ(doneJobs.size(), 0);
    }

} // namespace lms::core