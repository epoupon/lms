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

#include <cstddef>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <optional>

#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include "core/IChildProcess.hpp"

namespace lms::core
{
    class ChildProcess : public IChildProcess
    {
    public:
        ChildProcess(boost::asio::io_context& ioContext, const std::filesystem::path& path, const Args& args);
        ~ChildProcess() override;

        ChildProcess(const ChildProcess&) = delete;
        ChildProcess& operator=(const ChildProcess&) = delete;

    private:
        void asyncRead(std::byte* data, std::size_t bufferSize, ReadCallback callback) override;
        std::size_t readSome(std::byte* data, std::size_t bufferSize) override;
        bool finished() const override;

        void kill();
        bool wait(bool block); // return true if waited

        using FileDescriptor = boost::asio::posix::stream_descriptor;

        boost::asio::io_context& _ioContext;
        FileDescriptor _childStdout;
        ::pid_t _childPID{};
        bool _waited{};
        bool _finished{};
        std::optional<int> _exitCode;
    };
} // namespace lms::core