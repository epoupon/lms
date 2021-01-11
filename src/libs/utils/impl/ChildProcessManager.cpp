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

#include "ChildProcessManager.hpp"

#include "utils/Logger.hpp"

#include "ChildProcess.hpp"


std::unique_ptr<IChildProcessManager>
createChildProcessManager()
{
	return std::make_unique<ChildProcessManager>();
}

ChildProcessManager::ChildProcessManager()
: _work {boost::asio::make_work_guard(_ioContext)}
{
	start();
}

ChildProcessManager::~ChildProcessManager()
{
	stop();
}

void
ChildProcessManager::start()
{
	LMS_LOG(CHILDPROCESS, INFO) << "Starting child process manager...";

	_thread = std::make_unique<std::thread>([&]()
	{
		_ioContext.run();
	});

	LMS_LOG(CHILDPROCESS, INFO) << "Child process manager started!";
}

void
ChildProcessManager::stop()
{
	LMS_LOG(CHILDPROCESS, INFO) << "Stopping child process manager";
	_work.reset();
	_thread->join();
	LMS_LOG(CHILDPROCESS, INFO) << "Stopped child process manager";
}

std::unique_ptr<IChildProcess>
ChildProcessManager::spawnChildProcess(const std::filesystem::path& path, const IChildProcess::Args& args)
{
	return std::make_unique<ChildProcess>(_ioContext, path, args);
}


