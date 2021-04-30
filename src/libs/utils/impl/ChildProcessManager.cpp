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
createChildProcessManager(boost::asio::io_context& ioContext)
{
	return std::make_unique<ChildProcessManager>(ioContext);
}

ChildProcessManager::ChildProcessManager(boost::asio::io_context& ioContext)
: _ioContext {ioContext}
{
}

std::unique_ptr<IChildProcess>
ChildProcessManager::spawnChildProcess(const std::filesystem::path& path, const IChildProcess::Args& args)
{
	return std::make_unique<ChildProcess>(_ioContext, path, args);
}


