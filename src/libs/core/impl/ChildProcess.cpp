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

#include "ChildProcess.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <stdexcept>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <mutex>

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>

#include "core/Exception.hpp"
#include "core/ILogger.hpp"

namespace lms::core
{
    namespace
    {
        class SystemException : public ChildProcessException
        {
        public:
            SystemException(int err, const std::string& errMsg)
                : ChildProcessException{ errMsg + ": " + ::strerror(err) }
            {
            }

            SystemException(boost::system::error_code ec, const std::string& errMsg)
                : ChildProcessException{ errMsg + ": " + ec.message() }
            {
            }
        };
    } // namespace

    ChildProcess::ChildProcess(boost::asio::io_context& ioContext, const std::filesystem::path& path, const Args& args)
        : _ioContext{ ioContext }
        , _childStdout{ _ioContext }

    {
        // make sure only one thread is executing this part of code
        static std::mutex mutex;
        std::unique_lock<std::mutex> lock{ mutex };

        int pipefd[2];

        // Use 'pipe' instead of 'pipe2', more portable
        if (pipe(pipefd) < 0)
            throw SystemException{ errno, "pipe failed!" };

        // Manually set the O_NONBLOCK and O_CLOEXEC flags for both ends of the pipe
        if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == -1)
            throw SystemException{ errno, "fcntl failed to set O_NONBLOCK!" };

        if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) == -1)
            throw SystemException{ errno, "fcntl failed to set O_NONBLOCK!" };

        if (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) == -1)
            throw SystemException{ errno, "fcntl failed to set FD_CLOEXEC!" };

        if (fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) == -1)
            throw SystemException{ errno, "fcntl failed to set FD_CLOEXEC!" };

#if defined(__linux__) && defined(F_SETPIPE_SZ)
        {
            // Just a hint here to prevent the writer from writing too many bytes ahead of the reader
            constexpr std::size_t pipeSize{ 65536 * 4 };

            if (fcntl(pipefd[0], F_SETPIPE_SZ, pipeSize) == -1)
                throw SystemException{ errno, "fcntl failed!" };
            if (fcntl(pipefd[1], F_SETPIPE_SZ, pipeSize) == -1)
                throw SystemException{ errno, "fcntl failed!" };
        }
#endif

        int res{ fork() };
        if (res == -1)
            throw SystemException{ errno, "fork failed!" };

        if (res == 0) // CHILD
        {
            close(pipefd[0]);
            close(STDIN_FILENO);
            close(STDERR_FILENO);

            // Replace stdout with pipe write
            if (dup2(pipefd[1], STDOUT_FILENO) == -1)
                exit(-1);

            std::vector<const char*> execArgs;
            std::transform(std::cbegin(args), std::cend(args), std::back_inserter(execArgs), [](const std::string& arg) { return arg.c_str(); });
            execArgs.push_back(nullptr);

            res = execv(path.string().c_str(), (char* const*)&execArgs[0]);
            if (res == -1)
                exit(-1);
        }
        else // PARENT
        {
            close(pipefd[1]);
            {
                boost::system::error_code assignError;
                _childStdout.assign(pipefd[0], assignError);
                if (assignError)
                    throw SystemException{ assignError, "fork failed!" };
            }
            _childPID = res;
        }
    }

    ChildProcess::~ChildProcess()
    {
        LMS_LOG(CHILDPROCESS, DEBUG, "Closing child process...");
        {
            boost::system::error_code closeError;
            _childStdout.close(closeError);
            if (closeError)
                LMS_LOG(CHILDPROCESS, ERROR, "Closed failed: " << closeError.message());
        }

        if (!_finished)
            kill();

        wait(true);
    }

    void ChildProcess::kill()
    {
        // process may already have finished
        LMS_LOG(CHILDPROCESS, DEBUG, "Killing child process...");
        if (::kill(_childPID, SIGKILL) == -1)
            LMS_LOG(CHILDPROCESS, DEBUG, "Kill failed: " << ::strerror(errno));
    }

    bool ChildProcess::wait(bool block)
    {
        assert(!_waited);

        int wstatus{};
        const pid_t pid{ waitpid(_childPID, &wstatus, block ? 0 : WNOHANG) };

        if (pid == -1)
            throw SystemException{ errno, "waitpid failed!" };
        else if (pid == 0)
            return false;

        if (WIFEXITED(wstatus))
        {
            _exitCode = WEXITSTATUS(wstatus);
            LMS_LOG(CHILDPROCESS, DEBUG, "Exit code = " << *_exitCode);
        }

        _waited = true;
        return true;
    }

    void ChildProcess::asyncRead(std::byte* data, std::size_t bufferSize, ReadCallback callback)
    {
        assert(!finished());

        LMS_LOG(CHILDPROCESS, DEBUG, "Async read, bufferSize = " << bufferSize);

        boost::asio::async_read(_childStdout, boost::asio::buffer(data, bufferSize),
            [this, callback{ std::move(callback) }](const boost::system::error_code& error, std::size_t bytesTransferred) {
                LMS_LOG(CHILDPROCESS, DEBUG, "Async read cb - ec = '" << error.message() << "' (" << error.value() << "), bytesTransferred = " << bytesTransferred);

                ReadResult readResult{ ReadResult::Success };
                if (error)
                {
                    if (error != boost::asio::error::eof)
                    {
                        // forbidden to read any captured param here as the ChildProcess instance may already have been killed
                        return;
                    }

                    readResult = ReadResult::EndOfFile;
                    _finished = true;
                }

                callback(readResult, bytesTransferred);
            });
    }

    std::size_t ChildProcess::readSome(std::byte* data, std::size_t bufferSize)
    {
        boost::system::error_code ec;
        const std::size_t res{ _childStdout.read_some(boost::asio::buffer(data, bufferSize), ec) };
        LMS_LOG(CHILDPROCESS, DEBUG, "read some " << res << " bytes, ec = " << ec.message());
        if (ec)
            _childStdout.close(ec);

        return res;
    }

    bool ChildProcess::finished() const
    {
        return _finished;
    }
} // namespace lms::core