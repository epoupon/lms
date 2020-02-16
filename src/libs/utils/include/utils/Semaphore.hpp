/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <mutex>
#include <condition_variable>

class Semaphore
{
	public:
		Semaphore() = default;
		Semaphore(const Semaphore&) = delete;
		Semaphore(Semaphore&&) = delete;
		Semaphore& operator=(const Semaphore&) = delete;
		Semaphore& operator=(Semaphore&&) = delete;

		void notify()
		{
			std::unique_lock<std::mutex> lock {_mutex};

			_count++;
			_cv.notify_one();
		}

		void wait()
		{
			std::unique_lock<std::mutex> lock(_mutex);

			while (_count == 0)
				_cv.wait(lock);

			_count--;
		}

	private:
		std::mutex			_mutex;
		std::condition_variable		_cv;
		unsigned			_count {};
};


