/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "utils/RecursiveSharedMutex.hpp"

#include <cassert>

void
RecursiveSharedMutex::lock()
{
	if (_uniqueOwner == std::this_thread::get_id())
	{
		// already locked
		_uniqueCount++;
	}
	else
	{
		_mutex.lock();
		_uniqueOwner = std::this_thread::get_id();
		assert(_uniqueCount == 0);
		_uniqueCount = 1;
	}
}

void
RecursiveSharedMutex::unlock()
{
	assert(_uniqueCount > 0);

	if (--_uniqueCount == 0)
	{
		_uniqueOwner = {};
		_mutex.unlock();
	}
}

void
RecursiveSharedMutex::lock_shared()
{
	if (_uniqueOwner == std::this_thread::get_id())
	{
		// alone here, no need to lock
		_sharedCounts[std::this_thread::get_id()]++;

		return;
	}

	bool needLock {};

	{
		std::scoped_lock lock {_sharedCountMutex};

		auto& sharedCount {_sharedCounts[std::this_thread::get_id()]};
		if (sharedCount == 0)
			needLock = true;
		else
			++sharedCount;
	}

	if (needLock)
	{
		_mutex.lock_shared();

		assert(_uniqueOwner == std::thread::id {});

		std::scoped_lock lock {_sharedCountMutex};
		_sharedCounts[std::this_thread::get_id()]++;
	}
}

void
RecursiveSharedMutex::unlock_shared()
{
	if (_uniqueOwner == std::this_thread::get_id())
	{
		// alone here, no need to lock
		auto& sharedCount {_sharedCounts[std::this_thread::get_id()]};
		assert(sharedCount > 0);
		--sharedCount;

		return;
	}

	bool needUnlock {};

	{
		std::scoped_lock lock {_sharedCountMutex};

		auto& sharedCount {_sharedCounts[std::this_thread::get_id()]};
		assert(sharedCount > 0);
		needUnlock = (--sharedCount == 0);
	}

	if (needUnlock)
		_mutex.unlock_shared();
}
