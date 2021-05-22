/*
 * Copyright (C) 2019 Emeric Poupon
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
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "utils/RecursiveSharedMutex.hpp"

TEST(RecursiveSharedMutex, SingleThreaded)
{
	{
		RecursiveSharedMutex mutex;

		{
			std::unique_lock lock {mutex};
		}

		{
			std::shared_lock lock {mutex};
		}

		{
			std::unique_lock lock1 {mutex};
			std::unique_lock lock2 {mutex};
		}

		{
			std::shared_lock lock1 {mutex};
			std::shared_lock lock2 {mutex};
		}

		{
			std::unique_lock lock1 {mutex};
			std::shared_lock lock2 {mutex};
		}
	}
}

TEST(RecursiveSharedMutex, MultiThreaded)
{
	{
		constexpr std::size_t nbThreads {10};
		std::vector<std::thread> threads;

		RecursiveSharedMutex mutex;
		std::atomic<std::size_t> nbUnique {};
		std::atomic<std::size_t> nbShared {};
		for (std::size_t i {}; i < nbThreads; ++i)
		{
			threads.emplace_back([&]
			{
				{
					std::unique_lock lock {mutex};

					std::shared_lock lock2 {mutex};

					assert(nbUnique == 0);
					assert(nbShared == 0);
					nbUnique++;
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
					assert(nbUnique == 1);
					assert(nbShared == 0);
					nbUnique--;
				}

				{
					std::shared_lock lock {mutex};
					std::shared_lock lock2 {mutex};

					assert(nbUnique == 0);
					nbShared++;

					std::this_thread::sleep_for(std::chrono::milliseconds(15));

					assert(nbShared > 0);
					assert(nbShared <= nbThreads);
					assert(nbUnique == 0);
					nbShared--;
				}
			});
		}

		for (std::thread& t : threads)
			t.join();
	}
}
