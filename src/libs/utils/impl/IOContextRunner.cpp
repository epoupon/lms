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

#include "utils/IOContextRunner.hpp"

#include <cstdlib>

#include "utils/Logger.hpp"

IOContextRunner::IOContextRunner(boost::asio::io_service& ioService, std::size_t threadCount)
: _ioService {ioService}
, _work {ioService}
{
	LMS_LOG(UTILS, INFO) << "Starting IO context with " << threadCount << " threads...";
	for (std::size_t i {}; i < threadCount; ++i)
	{
		_threads.emplace_back([&]
		{
			try
			{
				_ioService.run();
			}
			catch (const std::exception& e)
			{
				LMS_LOG(UTILS, FATAL) << "Exception caught in IO context: " << e.what();
				std::abort();
			}
		});
	}
}

void
IOContextRunner::stop()
{
	LMS_LOG(UTILS, DEBUG) << "Stopping IO context...";
	_work.reset();
	_ioService.stop();
	LMS_LOG(UTILS, DEBUG) << "IO context stopped!";
}

IOContextRunner::~IOContextRunner()
{
	stop();

	for (std::thread& t : _threads)
		t.join();
}
