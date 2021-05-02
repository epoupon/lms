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
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <stdlib.h>

#include "utils/RecursiveSharedMutex.hpp"
#include "utils/String.hpp"


void
testStrings()
{
	{
		const std::string test{"a"};

		const std::vector<std::string_view> strings {StringUtils::splitString(test, "")};
		assert(strings.size() == 1);
		assert(strings.front() == "a");
	}

	{
		const std::string test{"a b"};

		const std::vector<std::string_view> strings {StringUtils::splitString(test, "|")};
		assert(strings.size() == 1);
		assert(strings.front() == "a b");
	}

	{
		const std::string test{"  a"};

		const std::vector<std::string_view> strings {StringUtils::splitString(test, " ")};
		assert(strings.size() == 1);
		assert(strings.front() == "a");
	}

	{
		const std::string test{"a  "};

		const std::vector<std::string_view> strings {StringUtils::splitString(test, " ")};
		assert(strings.size() == 1);
		assert(strings.front() == "a");
	}

	{
		const std::string test{"a b"};

		const std::vector<std::string_view> strings {StringUtils::splitString(test, " ")};
		assert(strings.size() == 2);
		assert(strings.front() == "a");
		assert(strings.back() == "b");
	}

	{
		const std::string test{"a b,c|defgh  "};

		const std::vector<std::string_view> strings {StringUtils::splitString(test, " ,|")};
		assert(strings.size() == 4);
		assert(strings[0] == "a");
		assert(strings[1] == "b");
		assert(strings[2] == "c");
		assert(strings[3] == "defgh");
	}

	{
		assert(StringUtils::escapeString("", "*", ' ') == "");
		assert(StringUtils::escapeString("", "", ' ') == "");
		assert(StringUtils::escapeString("a", "", ' ') == "a");
		assert(StringUtils::escapeString("*", "*", '_') == "_*");
		assert(StringUtils::escapeString("*a*", "*", '_') == "_*a_*");
		assert(StringUtils::escapeString("*a|", "*|", '_') == "_*a_|");
		assert(StringUtils::escapeString("**||", "*|", '_') == "_*_*_|_|");
	}
}

void
testSharedMutex()
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


int main()
{
	try
	{
		testStrings();
		testSharedMutex();
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
