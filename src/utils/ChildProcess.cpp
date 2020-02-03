#include "ChildProcess.hpp"

#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <algorithm>
#include <iostream>
#include <mutex>

#include <boost/asio/read.hpp>
#include <boost/asio/buffer.hpp>

#include "utils/Exception.hpp"
#include "utils/Logger.hpp"


ChildProcess::ChildProcess(boost::asio::io_context& ioContext, const std::filesystem::path& path, const Args& args)
: _ioContext {ioContext}
, _childStdout {_ioContext}

{
	// make sure only one thread is executing this part of code
	static std::mutex mutex;
	std::unique_lock<std::mutex> lock {mutex};

	int pipe[2];
	
	int res {pipe2(pipe, O_NONBLOCK | O_CLOEXEC)};
	if (res < 0)
		throw SystemException {errno, "pipe2 failed!"};

	res = fork();
	if (res == -1)
		throw SystemException {errno, "fork failed!"};

	if (res == 0) // CHILD
	{
		close(pipe[0]);
		close(STDIN_FILENO);
		close(STDERR_FILENO);

		// Replace stdout with pipe write
		if (dup2(pipe[1], STDOUT_FILENO) == -1)
			exit(-1);

		const char* execArgs[args.size() + 1] = {};
		std::transform(std::cbegin(args), std::cend(args), execArgs, [](const std::string& arg) { return arg.c_str(); });
		execArgs[args.size()] = NULL;

		res = execv(path.string().c_str(), (char *const*)execArgs);
		if (res == -1)
			exit(-1);
	}
	else // PARENT
	{
		close(pipe[1]);
		_childStdout.assign(pipe[0]);
		_childPID = res;
	}
}

ChildProcess::~ChildProcess()
{
	if (!_waited)
	{
		close(_childStdout.native_handle());
		kill();
		wait(true);
	}
}

void
ChildProcess::drain()
{
	char buf[128];

	while (boost::asio::read(_childStdout, boost::asio::buffer(buf)) > 0)
		LMS_LOG(CHILDPROCESS, DEBUG) << "drained some bytes" << std::endl;
}

void
ChildProcess::kill()
{
	::kill(_childPID, SIGKILL);
}

bool
ChildProcess::wait(bool block)
{
	int wstatus {};

	pid_t pid {waitpid(_childPID, &wstatus, block ? 0 : WNOHANG)};

	if (pid == -1)
		throw SystemException {errno, "waitpid failed!"};
	else if (pid == 0)
		return false;

	if (WIFEXITED(wstatus))
		_exitCode = WEXITSTATUS(wstatus);

	_waited = true;
	return true;
}


void
ChildProcess::asyncRead(unsigned char* data, std::size_t bufferSize, ReadCallback callback) 
{
	LMS_LOG(CHILDPROCESS, DEBUG) << "ASYNC READ";

	boost::asio::async_read(_childStdout, boost::asio::buffer(data, bufferSize),
		[callback {std::move(callback)}](const boost::system::error_code& error, std::size_t bytesTransferred)
		{
			LMS_LOG(CHILDPROCESS, DEBUG) << "ASYNC READ CB - error = '" << error.message() << "', bytesTransferred = " << bytesTransferred;

			if (error)
			{
				if (error == boost::asio::error::operation_aborted)
				{
					return;
				}
				if (error == boost::asio::error::eof)
				{
					callback(ReadResult::EndOfFile, bytesTransferred);
					return;
				}
				else
				{
					callback(ReadResult::Error, bytesTransferred);
					return;
				}
			}

			callback(ReadResult::Success, bytesTransferred);
		});
}

