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

#include <memory>

#include <boost/asio/io_context.hpp>

#include "core/IChildProcessManager.hpp"

namespace lms::core
{
    class ChildProcessManager : public IChildProcessManager
    {
    public:
        ChildProcessManager(boost::asio::io_context& ioContext);
        ~ChildProcessManager() override = default;

        ChildProcessManager(const ChildProcessManager&) = delete;
        ChildProcessManager(ChildProcessManager&&) = delete;
        ChildProcessManager& operator=(const ChildProcessManager&) = delete;
        ChildProcessManager& operator=(ChildProcessManager&&) = delete;

    private:
        std::unique_ptr<IChildProcess> spawnChildProcess(const std::filesystem::path& path, const IChildProcess::Args& args) override;

        boost::asio::io_context& _ioContext;
    };
} // namespace lms::core