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

#include <functional>
#include <thread>
#include <boost/asio/io_context.hpp>

template <typename It, typename Func>
void parallel_foreach(std::size_t nbWorkers, It begin, It end, Func&& func)
{
	if (nbWorkers == 0)
		throw std::runtime_error("Invalid worker count");

	boost::asio::io_context ioContext;

	for (It it {begin}; it != end; ++it)
	{
		auto refValue {std::ref<typename It::value_type>(*it)};
		ioContext.post([refValue, &func]() { std::cout << "EXEC FROM WORKER" << std::endl; func(refValue); std::cout << "END EXEC FROM WORKER" << std::endl; });
	}

	std::vector<std::thread> threads;
	for (std::size_t i {}; i < nbWorkers - 1; ++i)
		threads.emplace_back([&]() { ioContext.run(); });

	ioContext.run();

	for (std::thread& t : threads)
		t.join();
}

