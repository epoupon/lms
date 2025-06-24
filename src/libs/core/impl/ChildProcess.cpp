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

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <mutex>
#include <system_error>

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>

#include "core/ILogger.hpp"

namespace lms::core
{
    namespace
    {
        class SystemException : public ChildProcessException
        {
        public:
            SystemException(std::error_code err, const std::string& errMsg)
                : ChildProcessException{ errMsg + ": " + err.message() }
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
        const std::scoped_lock lock{ mutex };

        int pipefd[2];

        // Use 'pipe' instead of 'pipe2', more portable
        if (pipe(pipefd) == -1)
            throw SystemException{ std::error_code{ errno, std::generic_category() }, "pipe failed!" };

        // Only set O_NONBLOCK on read end - usually programs don't expect stdout to be non-blocking
        if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == -1)
            throw SystemException{ std::error_code{ errno, std::generic_category() }, "fcntl failed to set O_NONBLOCK!" };

#if defined(__linux__) && defined(F_SETPIPE_SZ)
        for (const int fd : { pipefd[0], pipefd[1] })
        {
            constexpr int targetPipeSize{ 65'536 * 4 };
            int currentPipeSize{ 65'536 }; // common default value
    #if defined(F_GETPIPE_SZ)
            const int pipeSizeRes{ fcntl(fd, F_GETPIPE_SZ) };
            if (pipeSizeRes == -1)
            {
                const int err{ errno };
                LMS_LOG(CHILDPROCESS, DEBUG, "F_GETPIPE_SZ failed: " << (std::error_code{ err, std::generic_category() }.message()));
            }
            else
            {
                currentPipeSize = pipeSizeRes;
            }
    #endif
            if (currentPipeSize < targetPipeSize)
            {
                if (::fcntl(fd, F_SETPIPE_SZ, targetPipeSize) == -1)
                {
                    const int err{ errno };
                    LMS_LOG(CHILDPROCESS, DEBUG, "F_SETPIPE_SZ failed: " << (std::error_code{ err, std::generic_category() }.message()));
                }
            }
        }
#endif

        const int res{ fork() };
        if (res == -1)
            throw SystemException{ std::error_code{ errno, std::generic_category() }, "fork failed!" };

        if (res == 0) // CHILD
        {
            // Never close stdin/out/err, most programs expect these to exist;
            // rather connect them to /dev/null if unwanted
            const int nullFd{ open("/dev/null", O_RDWR) };
            // Ignore errors, worst thing is stderr writes to the same fd as lms
            if (nullFd != -1)
            {
                dup2(nullFd, STDIN_FILENO);
                dup2(nullFd, STDERR_FILENO);
                close(nullFd);
            }

            // Replace stdout with pipe write
            if (dup2(pipefd[1], STDOUT_FILENO) == -1)
                exit(-1);
            // Close pipe: read end not needed, write end was dup2ed
            close(pipefd[0]);
            close(pipefd[1]);

            std::vector<const char*> execArgs;
            std::transform(std::cbegin(args), std::cend(args), std::back_inserter(execArgs), [](const std::string& arg) { return arg.c_str(); });
            execArgs.push_back(nullptr);

            execv(path.string().c_str(), (char* const*)&execArgs[0]);
            exit(-1);
        }
        else // PARENT
        {
            close(pipefd[1]);
            {
                boost::system::error_code assignError;
                _childStdout.assign(pipefd[0], assignError);
                if (assignError)
                    throw SystemException{ assignError, "assigning read end of pipe to asio stream failed!" };
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
        {
            const int err{ errno };
            LMS_LOG(CHILDPROCESS, DEBUG, "Kill failed: " << (std::error_code{ err, std::generic_category() }.message()));
        }
    }

    bool ChildProcess::wait(bool block)
    {
        assert(!_waited);

        int wstatus{};
        const pid_t pid{ waitpid(_childPID, &wstatus, block ? 0 : WNOHANG) };

        if (pid == -1)
            throw SystemException{ std::error_code{ errno, std::generic_category() }, "waitpid failed!" };
        if (pid == 0)
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